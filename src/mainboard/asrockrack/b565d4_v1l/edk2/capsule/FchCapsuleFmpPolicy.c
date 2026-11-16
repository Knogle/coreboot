/** @file
  Board-only FMP policy for the mirrored B565D4-V1L 32-MiB image.
  This source is linked only by an explicitly selected update build.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/SmmStoreLib.h>
#include "FchCapsuleBackend.h"
#include "FchCapsulePolicy.h"

STATIC EFI_STATUS LoadAndValidate (
  IN CONST VOID *Image,
  IN UINTN ImageSize,
  OUT UINT8 **Current
  )
{
  EFI_STATUS Status;
  UINTN BlockSize;
  UINTN NumBlocks;
  UINT8 *Before;
  struct b565d4_capsule_audit Audit;

  *Current = NULL;
  if (Image == NULL || ImageSize != B565D4_CAPSULE_BYTES) {
    return EFI_ABORTED;
  }
  Status = SmmStoreLibInitialize ();
  if (EFI_ERROR (Status)) { return Status; }
  Status = SmmStoreLibGetBlockSize (&BlockSize);
  if (EFI_ERROR (Status) || BlockSize != B565D4_CAPSULE_SECTOR_BYTES) {
    return EFI_UNSUPPORTED;
  }
  Status = SmmStoreLibGetNumBlocks (&NumBlocks);
  if (EFI_ERROR (Status) || NumBlocks !=
      B565D4_CAPSULE_BYTES / B565D4_CAPSULE_SECTOR_BYTES) {
    return EFI_UNSUPPORTED;
  }
  Before = AllocatePool (B565D4_CAPSULE_BYTES);
  if (Before == NULL) { return EFI_OUT_OF_RESOURCES; }
  for (UINTN Lba = 0; Lba < NumBlocks; Lba++) {
    UINTN NumBytes = BlockSize;

    Status = SmmStoreLibReadAnyBlock (Lba, 0, &NumBytes,
               Before + Lba * BlockSize);
    if (EFI_ERROR (Status) || NumBytes != BlockSize) {
      FreePool (Before);
      return EFI_DEVICE_ERROR;
    }
  }
  if (b565d4_capsule_validate (Before, Image, ImageSize, &Audit) !=
      B565D4_CAPSULE_OK) {
    DEBUG ((DEBUG_ERROR, "B565D4 capsule image rejected at 0x%x\n",
      Audit.first_bad_offset));
    FreePool (Before);
    return EFI_ABORTED;
  }
  *Current = Before;
  return EFI_SUCCESS;
}

EFI_STATUS B565D4CapsuleCheckImage (IN CONST VOID *Image, IN UINTN ImageSize)
{
  UINT8 *Current;
  EFI_STATUS Status = LoadAndValidate (Image, ImageSize, &Current);

  if (!EFI_ERROR (Status)) { FreePool (Current); }
  return Status;
}

EFI_STATUS B565D4CapsuleSetImage (
  IN CONST VOID *Image,
  IN UINTN ImageSize,
  IN EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS Progress OPTIONAL
  )
{
  UINT8 *Current;
  UINT8 Merged[B565D4_CAPSULE_SECTOR_BYTES];
  EFI_STATUS Status = LoadAndValidate (Image, ImageSize, &Current);
  BOOLEAN ReportProgress = Progress != NULL;

  if (EFI_ERROR (Status)) { return Status; }
  /* Advance lower and upper mirrors together, keeping any interrupted update
   * limited to one 4-KiB mirror pair. No retry or BIOS/full-flash fallback.
   */
  for (UINT32 Low = 0; Low < B565D4_CAPSULE_BANK_BYTES;
       Low += B565D4_CAPSULE_SECTOR_BYTES) {
    for (UINT32 Bank = 0; Bank < B565D4_CAPSULE_BYTES;
         Bank += B565D4_CAPSULE_BANK_BYTES) {
      UINT32 At = Low + Bank;
      int Changed = b565d4_capsule_merge_sector (Current, Image,
                      ImageSize, At, Merged);

      if (Changed < 0) { Status = EFI_ABORTED; goto Done; }
      if (Changed > 0) {
        Status = B565D4CapsuleWriteSector (At, Current + At, Merged);
        if (EFI_ERROR (Status)) { goto Done; }
      }
    }
    if (ReportProgress) {
      UINTN Percent = ((UINTN)(Low + B565D4_CAPSULE_SECTOR_BYTES) * 100) /
                      B565D4_CAPSULE_BANK_BYTES;
      if (Percent == 0) { Percent = 1; }
      if (EFI_ERROR (Progress (Percent))) { ReportProgress = FALSE; }
    }
  }
  Status = EFI_SUCCESS;
Done:
  FreePool (Current);
  return Status;
}
