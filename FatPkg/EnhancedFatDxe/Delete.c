/*++

Copyright (c) 2005, Intel Corporation
All rights reserved. This program and the accompanying materials
are licensed and made available under the terms and conditions of the Software
License Agreement which accompanies this distribution.


Module Name:

  delete.c

Abstract:

  Function that deletes a file

Revision History

--*/

#include "Fat.h"

EFI_STATUS
EFIAPI
FatDelete (
  IN EFI_FILE  *FHand
  )
/*++

Routine Description:

  Deletes the file & Closes the file handle.

Arguments:

  FHand                    - Handle to the file to delete.

Returns:

  EFI_SUCCESS              - Delete the file successfully.
  EFI_WARN_DELETE_FAILURE  - Fail to delete the file.

--*/
{
  FAT_IFILE   *IFile;
  FAT_OFILE   *OFile;
  FAT_DIRENT  *DirEnt;
  EFI_STATUS  Status;
  UINTN       Round;

  IFile = IFILE_FROM_FHAND (FHand);
  OFile = IFile->OFile;

  //
  // Lock the volume
  //
  FatAcquireLock ();

  //
  // If the file is read-only, then don't delete it
  //
  if (IFile->ReadOnly) {
    Status = EFI_WRITE_PROTECTED;
    goto Done;
  }
  //
  // If the file is the root dir, then don't delete it
  //
  if (OFile->Parent == NULL) {
    Status = EFI_ACCESS_DENIED;
    goto Done;
  }
  //
  // If the file has a permanant error, skip the delete
  //
  Status = OFile->Error;
  if (!EFI_ERROR (Status)) {
    //
    // If this is a directory, make sure it's empty before
    // allowing it to be deleted
    //
    if (OFile->ODir != NULL) {
      //
      // We do not allow to delete nonempty directory
      //
      FatResetODirCursor (OFile);
      for (Round = 0; Round < 3; Round++) {
        Status = FatGetNextDirEnt (OFile, &DirEnt);
        if ((EFI_ERROR (Status)) ||
            ((Round < 2) && (DirEnt == NULL || !FatIsDotDirEnt (DirEnt))) ||
            ((Round == 2) && (DirEnt != NULL))
            ) {
          Status = EFI_ACCESS_DENIED;
          goto Done;
        }
      }
    }
    //
    // Return the file's space by setting its size to 0
    //
    FatTruncateOFile (OFile, 0);
    //
    // Free the directory entry for this file
    //
    Status = FatRemoveDirEnt (OFile->Parent, OFile->DirEnt);
    if (EFI_ERROR (Status)) {
      goto Done;
    }
    //
    // Set a permanent error for this OFile in case there
    // are still opened IFiles attached
    //
    OFile->Error = EFI_NOT_FOUND;
  } else if (OFile->Error == EFI_NOT_FOUND) {
    Status = EFI_SUCCESS;
  }

Done:
  //
  // Always close the handle
  //
  FatIFileClose (IFile);
  //
  // Done
  //
  Status = FatCleanupVolume (OFile->Volume, NULL, Status);
  FatReleaseLock ();

  if (EFI_ERROR (Status)) {
    Status = EFI_WARN_DELETE_FAILURE;
  }

  return Status;
}
