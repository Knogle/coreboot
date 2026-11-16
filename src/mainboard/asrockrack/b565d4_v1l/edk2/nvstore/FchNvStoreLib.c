/** @file
  Board-scoped runtime FVB backing on AMD FCH SPI100.

  Reuses the existing SmmStoreFvb protocol adapter and storage interface,
  but executes no SMI. Only the allocated upper NVAR volume is accessible.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Guid/SystemNvDataGuid.h>
#include <Library/BaseCryptLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PciLib.h>
#include <Library/PrintLib.h>
#include <Library/SerialPortLib.h>
#include <Library/SmmStoreLib.h>
#include <Library/SynchronizationLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "FchNvCore.h"

#define FCH_SPI_BASE  0xFEC10000U

STATIC UINT8    *mSpi;
STATIC UINT8    *mCache;
STATIC BOOLEAN  mReady;
STATIC BOOLEAN  mFaulted;
STATIC BOOLEAN  mSerialReady;
STATIC UINT32   mLock;
STATIC UINT64   mCounterMask;
STATIC UINT64   mLastCounter;
STATIC UINT64   mTicks;

// Finite initialization diagnostics also work in a RELEASE payload. Runtime
// variable operations use no unsolicited writes to the OS-owned console.
STATIC VOID InitTrace (CONST CHAR8 *Stage, UINT64 Value)
{
  CHAR8  Line[96];
  UINTN  Bytes;

  if (!mSerialReady) { return; }
  Bytes = AsciiSPrint (Line, sizeof (Line), "\r\nFCH_NV %a value=%Lx\r\n", Stage, Value);
  SerialPortWrite ((UINT8 *)Line, Bytes);
}

uint8_t fch_nv_io_read8(unsigned int offset)
{
  return MmioRead8 ((UINTN)mSpi + offset);
}

uint16_t fch_nv_io_read16(unsigned int offset)
{
  return MmioRead16 ((UINTN)mSpi + offset);
}

uint32_t fch_nv_io_read32(unsigned int offset)
{
  return MmioRead32 ((UINTN)mSpi + offset);
}

void fch_nv_io_write8(unsigned int offset, uint8_t value)
{
  MmioWrite8 ((UINTN)mSpi + offset, value);
}

void fch_nv_io_write16(unsigned int offset, uint16_t value)
{
  MmioWrite16 ((UINTN)mSpi + offset, value);
}

void fch_nv_io_delay(unsigned int microseconds)
{
  MicroSecondDelay (microseconds);
}

uint64_t fch_nv_io_time_us(void)
{
  UINT64  Now;

  // Calls are less than one ACPI-timer rollover apart during an operation.
  Now = GetPerformanceCounter () & mCounterMask;
  mTicks += (Now - mLastCounter) & mCounterMask;
  mLastCounter = Now;
  return GetTimeInNanoSecond (mTicks) / 1000;
}

int fch_nv_io_lock(void)
{
  if (InterlockedCompareExchange32 (&mLock, 0, 1) != 0) {
    return 0;
  }
  mLastCounter = GetPerformanceCounter () & mCounterMask;
  mTicks = 0;
  return 1;
}

void fch_nv_io_unlock(void)
{
  MemoryFence ();
  InterlockedCompareExchange32 (&mLock, 1, 0);
}

STATIC EFI_STATUS ResultStatus (enum fch_nv_result Result)
{
  if (Result == FCH_NV_IO_ERROR || Result == FCH_NV_TIMEOUT) {
    // Do not retry an unknown programming outcome until a new cold boot.
    mFaulted = TRUE;
  }
  switch (Result) {
    case FCH_NV_OK:          return EFI_SUCCESS;
    case FCH_NV_INVALID:     return EFI_INVALID_PARAMETER;
    case FCH_NV_BUSY:        return EFI_NOT_READY;
    case FCH_NV_TIMEOUT:     return EFI_TIMEOUT;
    case FCH_NV_NEEDS_ERASE: return EFI_WRITE_PROTECTED;
    default:                return EFI_DEVICE_ERROR;
  }
}

EFI_STATUS SmmStoreLibGetBlockSize (OUT UINTN *BlockSize)
{
  if (BlockSize == NULL) { return EFI_INVALID_PARAMETER; }
  if (!mReady) { return EFI_NO_MEDIA; }
  *BlockSize = FCH_NV_BLOCK;
  return EFI_SUCCESS;
}

EFI_STATUS SmmStoreLibGetNumBlocks (OUT UINTN *NumBlocks)
{
  if (NumBlocks == NULL) { return EFI_INVALID_PARAMETER; }
  if (!mReady) { return EFI_NO_MEDIA; }
  *NumBlocks = FCH_NV_BYTES / FCH_NV_BLOCK;
  return EFI_SUCCESS;
}

EFI_STATUS SmmStoreLibGetMmioAddress (OUT EFI_PHYSICAL_ADDRESS *MmioAddress)
{
  if (MmioAddress == NULL) { return EFI_INVALID_PARAMETER; }
  if (!mReady) { return EFI_NO_MEDIA; }
  // VariableRuntimeDxe reads this runtime RAM mirror directly.
  *MmioAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)mCache;
  return EFI_SUCCESS;
}

STATIC BOOLEAN ValidBlock (EFI_LBA Lba, UINTN Offset, UINTN Bytes)
{
  return Lba < FCH_NV_BYTES / FCH_NV_BLOCK && Offset < FCH_NV_BLOCK &&
         Bytes != 0 && Bytes <= FCH_NV_BLOCK - Offset;
}

EFI_STATUS SmmStoreLibRead (EFI_LBA Lba, UINTN Offset, UINTN *NumBytes, UINT8 *Buffer)
{
  if (NumBytes == NULL || Buffer == NULL || !ValidBlock (Lba, Offset, *NumBytes)) {
    return EFI_INVALID_PARAMETER;
  }
  if (!mReady) { return EFI_NO_MEDIA; }
  if (mFaulted) { return EFI_DEVICE_ERROR; }
  return ResultStatus (fch_nv_read ((UINT32)(Lba * FCH_NV_BLOCK + Offset), Buffer, *NumBytes));
}

EFI_STATUS SmmStoreLibWrite (EFI_LBA Lba, UINTN Offset, UINTN *NumBytes, UINT8 *Buffer)
{
  UINT32              At;
  UINTN               Completed;
  enum fch_nv_result  Result;

  if (NumBytes == NULL || Buffer == NULL || !ValidBlock (Lba, Offset, *NumBytes)) {
    return EFI_INVALID_PARAMETER;
  }
  if (!mReady) { return EFI_NO_MEDIA; }
  if (mFaulted) { return EFI_DEVICE_ERROR; }
  At = (UINT32)(Lba * FCH_NV_BLOCK + Offset);
  Result = fch_nv_write (At, Buffer, *NumBytes, &Completed);
  // A failed partial operation still reports only verified programmed bytes.
  CopyMem (mCache + At, Buffer, Completed);
  *NumBytes = Completed;
  return ResultStatus (Result);
}

EFI_STATUS SmmStoreLibEraseBlock (EFI_LBA Lba)
{
  enum fch_nv_result  Result;

  if (Lba >= FCH_NV_BYTES / FCH_NV_BLOCK) { return EFI_INVALID_PARAMETER; }
  if (!mReady) { return EFI_NO_MEDIA; }
  if (mFaulted) { return EFI_DEVICE_ERROR; }
  Result = fch_nv_erase ((UINT32)(Lba * FCH_NV_BLOCK));
  if (Result == FCH_NV_OK) {
    SetMem (mCache + Lba * FCH_NV_BLOCK, FCH_NV_BLOCK, 0xFF);
  }
  return ResultStatus (Result);
}

// This provider deliberately has no whole-flash update/capsule interface.
EFI_STATUS SmmStoreLibReadAnyBlock (EFI_LBA Lba, UINTN Offset, UINTN *NumBytes, UINT8 *Buffer)
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS SmmStoreLibWriteAnyBlock (EFI_LBA Lba, UINTN Offset, UINTN *NumBytes, UINT8 *Buffer)
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS SmmStoreLibEraseAnyBlock (EFI_LBA Lba)
{
  return EFI_UNSUPPORTED;
}

VOID EFIAPI SmmStoreLibVirtualAddressChange (CONVERT_POINTER_CALLBACK ConvertPointer)
{
  ConvertPointer (0, (VOID **)&mSpi);
  ConvertPointer (0, (VOID **)&mCache);
}

STATIC BOOLEAN IsErased (CONST UINT8 *Buffer, UINTN Bytes)
{
  while (Bytes-- != 0) {
    if (*Buffer++ != 0xFF) { return FALSE; }
  }
  return TRUE;
}

EFI_STATUS SmmStoreLibInitialize (VOID)
{
  STATIC CONST UINT8 LegacyHash[32] = {
    0x4e,0x18,0xd5,0xc2,0xda,0xfa,0x73,0x2e,0x93,0x2c,0x17,0x7b,0x97,0x64,0x8a,0xd4,
    0x1a,0xae,0x9a,0x1a,0x4a,0x41,0xd6,0xe3,0x68,0x85,0x70,0xbd,0x21,0x81,0xf8,0x12
  };
  EFI_STATUS                       Status;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  Descriptor;
  EFI_FIRMWARE_VOLUME_HEADER      *Header;
  UINT8                            Hash[32];
  UINT64                           CounterStart;
  BOOLEAN                          KnownStore;
  UINT32                           LpcId;
  UINT32                           SpiBar;

  // The serial library constructor does not initialize its per-image UART
  // PCDs. Discover the existing bootloader UART before diagnostic writes.
  mSerialReady = !RETURN_ERROR (SerialPortInitialize ());
  InitTrace ("ENTER", 1);
  if (mFaulted) { return EFI_DEVICE_ERROR; }
  if (mReady) { return EFI_SUCCESS; }
  // Standard payload PCI access is needed only during initialization.
  // Runtime variable operations use only the converted SPI MMIO pointer.
  LpcId = PciRead32 (PCI_LIB_ADDRESS (0, 20, 3, 0));
  SpiBar = PciRead32 (PCI_LIB_ADDRESS (0, 20, 3, 0xA0));
  InitTrace ("LPC_ID", LpcId);
  InitTrace ("LPC_SPI", SpiBar);
  if (LpcId != 0x790E1022 || (SpiBar & 0xFFFFFFC0) != FCH_SPI_BASE) {
    InitTrace ("LPC_REJECT", EFI_UNSUPPORTED);
    return EFI_UNSUPPORTED;
  }
  if (GetPerformanceCounterProperties (&CounterStart, &mCounterMask) == 0 ||
      CounterStart != 0 || mCounterMask < 0xFFFFFF ||
      (mCounterMask & (mCounterMask + 1)) != 0) {
    InitTrace ("TIMER_REJECT", mCounterMask);
    return EFI_UNSUPPORTED;
  }
  mSpi = (UINT8 *)(UINTN)FCH_SPI_BASE;
  Status = gDS->GetMemorySpaceDescriptor (FCH_SPI_BASE, &Descriptor);
  InitTrace ("GCD_QUERY", Status);
  if (!EFI_ERROR (Status)) {
    InitTrace ("GCD_CAPS", Descriptor.Capabilities);
    InitTrace ("GCD_ATTRIBUTES", Descriptor.Attributes);
    InitTrace ("GCD_TYPE", Descriptor.GcdMemoryType);
  }
  if (EFI_ERROR (Status) || Descriptor.GcdMemoryType == EfiGcdMemoryTypeNonExistent) {
    Status = gDS->AddMemorySpace (EfiGcdMemoryTypeMemoryMappedIo, FCH_SPI_BASE, 0x1000,
                                EFI_MEMORY_UC | EFI_MEMORY_RUNTIME);
    InitTrace ("GCD_ADD", Status);
    if (EFI_ERROR (Status)) { return Status; }
  }
  // Coreboot owns the already operational FCH MMIO cache/routing policy.
  // UefiPayload deliberately disables MTRR reprogramming. A redundant UC
  // request therefore fails in CpuDxe even when the device mapping is valid.
  // GCD's runtime-only operation preserves all existing CPU attributes and
  // adds the page to the runtime memory map without changing MTRRs or PAT.
  Status = gDS->SetMemorySpaceAttributes (FCH_SPI_BASE, 0x1000, EFI_MEMORY_RUNTIME);
  InitTrace ("GCD_RUNTIME", Status);
  if (EFI_ERROR (Status)) { return Status; }
  Status = ResultStatus (fch_nv_probe ());
  InitTrace ("PROBE", Status);
  if (EFI_ERROR (Status)) { return Status; }
  mCache = AllocateRuntimePages (EFI_SIZE_TO_PAGES (FCH_NV_BYTES));
  InitTrace ("CACHE", (UINTN)mCache);
  if (mCache == NULL) { return EFI_OUT_OF_RESOURCES; }
  Status = ResultStatus (fch_nv_read (0, mCache, FCH_NV_BYTES));
  InitTrace ("READ_STORE", Status);
  if (EFI_ERROR (Status)) { goto Fail; }

  // Format only a recognized owned legacy volume or a completely erased one.
  // Later valid variable volumes are checked again by the common FVB driver.
  Header = (EFI_FIRMWARE_VOLUME_HEADER *)mCache;
  KnownStore = Header->Signature == EFI_FVH_SIGNATURE &&
               Header->FvLength == FCH_NV_BYTES &&
               Header->HeaderLength >= sizeof (*Header) &&
               Header->HeaderLength <= FCH_NV_BLOCK &&
               (Header->HeaderLength & 1) == 0 &&
               CompareGuid (&Header->FileSystemGuid, &gEfiSystemNvDataFvGuid) &&
               CalculateSum16 ((UINT16 *)Header, Header->HeaderLength) == 0;
  if (!KnownStore && IsErased (mCache, FCH_NV_BYTES)) {
    KnownStore = TRUE;
  }
  if (!KnownStore && Sha256HashAll (mCache, FCH_NV_BYTES, Hash) &&
      CompareMem (Hash, LegacyHash, sizeof (Hash)) == 0) {
    KnownStore = TRUE;
  }
  if (!KnownStore) {
    InitTrace ("UNKNOWN_STORE", EFI_COMPROMISED_DATA);
    Status = EFI_COMPROMISED_DATA;
    goto Fail;
  }
  mReady = TRUE;
  InitTrace ("PROVIDER_READY", FCH_NV_BYTES);
  DEBUG ((DEBUG_ERROR, "FCH NV: upper NVAR 01037000/00020000, runtime FVB, no SMI\n"));
  return EFI_SUCCESS;

Fail:
  FreePages (mCache, EFI_SIZE_TO_PAGES (FCH_NV_BYTES));
  mCache = NULL;
  return Status;
}

VOID EFIAPI SmmStoreLibDeinitialize (VOID)
{
  mReady = FALSE;
  if (mCache != NULL) {
    FreePages (mCache, EFI_SIZE_TO_PAGES (FCH_NV_BYTES));
    mCache = NULL;
  }
}
