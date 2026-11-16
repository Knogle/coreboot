/** @file
  Private B565D4-V1L FMP backend interface. Only an opt-in FmpDxe instance
  links this interface; the ordinary UEFI variable provider never does.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#ifndef B565D4_FCH_CAPSULE_BACKEND_H
#define B565D4_FCH_CAPSULE_BACKEND_H

#include <Uefi.h>
#include <Protocol/FirmwareManagement.h>

EFI_STATUS B565D4CapsuleCheckImage (
  IN CONST VOID *Image,
  IN UINTN ImageSize
  );

EFI_STATUS B565D4CapsuleSetImage (
  IN CONST VOID *Image,
  IN UINTN ImageSize,
  IN EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS Progress OPTIONAL
  );

/* Direct sector API is private to this FmpDxe image. Generic SmmStoreLib
 * erase/write entry points stay unsupported, eliminating generic fallback.
 */
EFI_STATUS B565D4CapsuleWriteSector (
  IN UINT32 Offset,
  IN CONST UINT8 *ExpectedBefore,
  IN CONST UINT8 *NewBytes
  );

#endif
