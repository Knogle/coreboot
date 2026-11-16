/** @file
  DXE-only, read-only 32-MiB FCH-SPI provider for B565D4-V1L FMP analysis.
  It intentionally has no capsule flash-write or erase implementation.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PciLib.h>
#include <Library/SmmStoreLib.h>
#include <Library/SynchronizationLib.h>
#include <Library/TimerLib.h>
#include "FchNvCore.h"
#ifdef FCH_NV_CAPSULE_UPDATE
#include "FchCapsuleBackend.h"
#include "FchCapsulePolicy.h"
#endif

#define FCH_SPI_BASE 0xFEC10000U

STATIC UINT8   *mSpi = (UINT8 *)(UINTN)FCH_SPI_BASE;
STATIC BOOLEAN mReady;
STATIC BOOLEAN mFaulted;
STATIC UINT32  mLock;
STATIC UINT64  mCounterMask;
STATIC UINT64  mLastCounter;
STATIC UINT64  mTicks;

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
  UINT64 Now;

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

STATIC EFI_STATUS ReadStatus (enum fch_nv_result Result)
{
  if (Result == FCH_NV_IO_ERROR || Result == FCH_NV_TIMEOUT) {
    mFaulted = TRUE;
  }
  switch (Result) {
    case FCH_NV_OK:      return EFI_SUCCESS;
    case FCH_NV_INVALID: return EFI_INVALID_PARAMETER;
    case FCH_NV_BUSY:    return EFI_NOT_READY;
    case FCH_NV_TIMEOUT: return EFI_TIMEOUT;
    default:             return EFI_DEVICE_ERROR;
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
  return EFI_UNSUPPORTED;
}

EFI_STATUS SmmStoreLibReadAnyBlock (EFI_LBA Lba, UINTN Offset, UINTN *NumBytes, UINT8 *Buffer)
{
  EFI_STATUS Status;

  if (NumBytes == NULL || Buffer == NULL || Lba >= FCH_NV_BYTES / FCH_NV_BLOCK ||
      Offset >= FCH_NV_BLOCK || *NumBytes == 0 || *NumBytes > FCH_NV_BLOCK - Offset) {
    return EFI_INVALID_PARAMETER;
  }
  if (!mReady) { return EFI_NO_MEDIA; }
  if (mFaulted) { return EFI_DEVICE_ERROR; }
  Status = ReadStatus (fch_nv_read ((UINT32)(Lba * FCH_NV_BLOCK + Offset), Buffer, *NumBytes));
  if (EFI_ERROR (Status)) { *NumBytes = 0; }
  return Status;
}

EFI_STATUS SmmStoreLibRead (EFI_LBA Lba, UINTN Offset, UINTN *NumBytes, UINT8 *Buffer)
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS SmmStoreLibWrite (EFI_LBA Lba, UINTN Offset, UINTN *NumBytes, UINT8 *Buffer)
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS SmmStoreLibEraseBlock (EFI_LBA Lba)
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
  // This library is linked only into the pre-ExitBootServices FmpDxe driver.
}

EFI_STATUS SmmStoreLibInitialize (VOID)
{
  UINT64 CounterStart;
  UINT32 LpcId;
  UINT32 SpiBar;
  EFI_STATUS Status;

  if (mFaulted) { return EFI_DEVICE_ERROR; }
  if (mReady) { return EFI_SUCCESS; }
  LpcId = PciRead32 (PCI_LIB_ADDRESS (0, 20, 3, 0));
  SpiBar = PciRead32 (PCI_LIB_ADDRESS (0, 20, 3, 0xA0));
  if (LpcId != 0x790E1022 || (SpiBar & 0xFFFFFFC0) != FCH_SPI_BASE) {
    return EFI_UNSUPPORTED;
  }
  if (GetPerformanceCounterProperties (&CounterStart, &mCounterMask) == 0 ||
      CounterStart != 0 || mCounterMask < 0xFFFFFF ||
      (mCounterMask & (mCounterMask + 1)) != 0) {
    return EFI_UNSUPPORTED;
  }
  Status = ReadStatus (fch_nv_probe ());
  if (EFI_ERROR (Status)) { return Status; }
  mReady = TRUE;
  DEBUG ((DEBUG_INFO, "FCH capsule probe: 32-MiB read-only SPI; writes disabled\n"));
  return EFI_SUCCESS;
}

VOID EFIAPI SmmStoreLibDeinitialize (VOID)
{
  mReady = FALSE;
}

#ifdef FCH_NV_CAPSULE_UPDATE
/* A single explicitly authorized sector operation. The caller has already
 * checked the whole current/candidate pair and prepared a merged sector.
 * Re-reading immediately before erase catches unexpected concurrent changes.
 */
EFI_STATUS B565D4CapsuleWriteSector (
  IN UINT32 Offset,
  IN CONST UINT8 *ExpectedBefore,
  IN CONST UINT8 *NewBytes
  )
{
  UINT8 Actual[FCH_NV_BLOCK];
  size_t Completed = 0;
  EFI_STATUS Status;

  if (ExpectedBefore == NULL || NewBytes == NULL ||
      (Offset & (FCH_NV_BLOCK - 1)) != 0 ||
      Offset > FCH_NV_BYTES - FCH_NV_BLOCK) {
    return EFI_INVALID_PARAMETER;
  }
  if (!b565d4_capsule_sector_delta_allowed (Offset, ExpectedBefore,
                                             NewBytes)) {
    return EFI_ABORTED;
  }
  if (!mReady) { return EFI_NO_MEDIA; }
  if (mFaulted) { return EFI_DEVICE_ERROR; }
  Status = ReadStatus (fch_nv_read (Offset, Actual, sizeof (Actual)));
  if (EFI_ERROR (Status)) { return Status; }
  if (CompareMem (Actual, ExpectedBefore, sizeof (Actual)) != 0) {
    return EFI_ABORTED;
  }
  if (CompareMem (Actual, NewBytes, sizeof (Actual)) == 0) {
    return EFI_SUCCESS;
  }
  Status = ReadStatus (fch_nv_erase (Offset));
  if (EFI_ERROR (Status)) { mFaulted = TRUE; return Status; }
  Status = ReadStatus (fch_nv_write (Offset, NewBytes, sizeof (Actual), &Completed));
  if (EFI_ERROR (Status) || Completed != sizeof (Actual)) {
    mFaulted = TRUE;
    return EFI_DEVICE_ERROR;
  }
  Status = ReadStatus (fch_nv_read (Offset, Actual, sizeof (Actual)));
  if (EFI_ERROR (Status) || CompareMem (Actual, NewBytes, sizeof (Actual)) != 0) {
    mFaulted = TRUE;
    return EFI_DEVICE_ERROR;
  }
  return EFI_SUCCESS;
}
#endif
