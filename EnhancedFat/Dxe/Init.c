/*++

Copyright (c) 2006, Intel Corporation
All rights reserved. This program and the accompanying materials
are licensed and made available under the terms and conditions of the Software 
License Agreement which accompanies this distribution.


Module Name:

  Init.c
  
Abstract:

  Initialization routines
  
--*/

#include "Fat.h"

EFI_STATUS
FatInitUnicodeCollationSupport (
  IN  EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN  LC_ISO_639_2                   *LangCode,
  OUT EFI_UNICODE_COLLATION_PROTOCOL **UnicodeCollationInterface
  )
/*++

Routine Description:

  Initializes Unicode Collation support.
    
Arguments:

  This                       - Protocol instance pointer.    
  LangCode                   - Language Code specified.
  UnicodeCollationInterface  - Unicode Collation protocol interface returned.

Returns:

  EFI_SUCCESS                - Successfully get the Unicode Collation protocol interface by 
                               specified LangCode.
  EFI_NOT_FOUND              - Specified Unicode Collation protocol is not found.

--*/
{
  EFI_STATUS                      Status;
  LC_ISO_639_2                    *Languages;
  UINTN                           Index;
  UINTN                           NoHandles;
  EFI_HANDLE                      *Handles;
  EFI_UNICODE_COLLATION_PROTOCOL  *Uci;
  EFI_UNICODE_COLLATION_PROTOCOL  *EngUci;
  EFI_UNICODE_COLLATION_PROTOCOL  *FoundUci;

  ASSERT (LangCode != NULL);
  //
  // Locate all Unicode Collation drivers.
  //
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiUnicodeCollationProtocolGuid,
                  NULL,
                  &NoHandles,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  FoundUci  = NULL;
  EngUci    = NULL;

  //
  // Check all Unicode Collation drivers for a matching language code.
  //
  for (Index = 0; Index < NoHandles; Index++) {
    //
    // Open Unicode Collation Protocol
    //
    Status = gBS->OpenProtocol (
                    Handles[Index],
                    &gEfiUnicodeCollationProtocolGuid,
                    (VOID **) &Uci,
                    This->DriverBindingHandle,
                    NULL,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );

    if (EFI_ERROR (Status)) {
      continue;
    }
    //
    // Check for a matching language code.
    //
    for (Languages = Uci->SupportedLanguages; *Languages != 0; Languages += LC_ISO_639_2_ENTRY_SIZE) {
      //
      // If this code matches, use this driver
      //
      if (EfiLibCompareLanguage (Languages, LangCode)) {
        FoundUci = Uci;
        goto Done;
      }

      if (EfiLibCompareLanguage (Languages, "eng")) {
        EngUci = Uci;
      }
    }
  }

Done:
  //
  // Cleanup
  //
  if (Handles != NULL) {
    gBS->FreePool (Handles);
  }

  if (FoundUci == NULL) {
    FoundUci = EngUci;
  }

  *UnicodeCollationInterface = FoundUci;
  return FoundUci != NULL ? EFI_SUCCESS : EFI_NOT_FOUND;
}

EFI_STATUS
FatAllocateVolume (
  IN  EFI_HANDLE                Handle,
  IN  EFI_DISK_IO_PROTOCOL      *DiskIo,
  IN  EFI_BLOCK_IO_PROTOCOL     *BlockIo
  )
/*++

Routine Description:
  
  Allocates volume structure, detects FAT file system, installs protocol,
  and initialize cache.
  
Arguments:
 
  Handle                - The handle of parent device.
  DiskIo                - The DiskIo of parent device.
  BlockIo               - The BlockIo of parent devicel
  
Returns:

  EFI_SUCCESS           - Allocate a new volume successfully.
  EFI_OUT_OF_RESOURCES  - Can not allocate the memory.
  Others                - Allocating a new volume failed.

--*/
{
  EFI_STATUS  Status;
  FAT_VOLUME  *Volume;
  BOOLEAN     LockedByMe;
  LockedByMe = FALSE;
  //
  // Allocate a volume structure
  //
  Volume = EfiLibAllocateZeroPool (sizeof (FAT_VOLUME));
  if (Volume == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  //
  // Acquire the lock.
  // If caller has already acquired the lock, cannot lock it again.
  //
  if (!FatIsLocked ()) {
    FatAcquireLock ();
    LockedByMe = TRUE;
  }
  //
  // Initialize the structure
  //
  Volume->Signature                   = FAT_VOLUME_SIGNATURE;
  Volume->Handle                      = Handle;
  Volume->DiskIo                      = DiskIo;
  Volume->BlockIo                     = BlockIo;
  Volume->MediaId                     = BlockIo->Media->MediaId;
  Volume->ReadOnly                    = BlockIo->Media->ReadOnly;
  Volume->VolumeInterface.Revision    = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION;
  Volume->VolumeInterface.OpenVolume  = FatOpenVolume;
  InitializeListHead (&Volume->CheckRef);
  InitializeListHead (&Volume->DirCacheList);
  //
  // Initialize Root Directory entry
  //
  Volume->RootDirEnt.FileString       = Volume->RootFileString;
  Volume->RootDirEnt.Entry.Attributes = FAT_ATTRIBUTE_DIRECTORY;
  //
  // Check to see if there's a file system on the volume
  //
  Status = FatOpenDevice (Volume);
  if (EFI_ERROR (Status)) {
    goto Done;
  }
  //
  // Initialize cache
  //
  Status = FatInitializeDiskCache (Volume);
  if (EFI_ERROR (Status)) {
    goto Done;
  }
  //
  // Install our protocol interfaces on the device's handle
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Volume->Handle,
                  &gEfiSimpleFileSystemProtocolGuid,
                  &Volume->VolumeInterface,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    goto Done;
  }
  //
  // Volume installed
  //
  DEBUG ((EFI_D_INIT, "%HInstalled Fat filesystem on %x%N\n", Handle));
  Volume->Valid = TRUE;

Done:
  //
  // Unlock if locked by myself.
  //
  if (LockedByMe) {
    FatReleaseLock ();
  }

  if (EFI_ERROR (Status)) {
    FatFreeVolume (Volume);
  }

  return Status;
}

EFI_STATUS
FatAbandonVolume (
  IN FAT_VOLUME *Volume
  )
/*++
  
Routine Description:
 
  Called by FatDriverBindingStop(), Abandon the volume.
  
Arguments:
 
  Volume                - The volume to be abandoned.
  
Returns:
 
  EFI_SUCCESS           - Abandoned the volume successfully.
  Others                - Can not uninstall the protocol interfaces.
  
--*/
{
  EFI_STATUS  Status;
  BOOLEAN     LockedByMe;

  //
  // Uninstall the protocol interface.
  //
  if (Volume->Handle != NULL) {
    Status = gBS->UninstallMultipleProtocolInterfaces (
                    Volume->Handle,
                    &gEfiSimpleFileSystemProtocolGuid,
                    &Volume->VolumeInterface,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  LockedByMe = FALSE;

  //
  // Acquire the lock.
  // If the caller has already acquired the lock (which
  // means we are in the process of some Fat operation),
  // we can not acquire again.
  //
  if (!FatIsLocked ()) {
    LockedByMe = TRUE;
    FatAcquireLock ();
  }
  //
  // The volume is still being used. Hence, set error flag for all OFiles still in
  // use. In two cases, we could get here. One is EFI_MEDIA_CHANGED, the other is
  // EFI_NO_MEDIA.
  //
  if (Volume->Root != NULL) {
    FatSetVolumeError (
      Volume->Root,
      Volume->BlockIo->Media->MediaPresent ? EFI_MEDIA_CHANGED : EFI_NO_MEDIA
      );
  }

  Volume->Valid = FALSE;

  //
  // Release the lock.
  // If locked by me, this means DriverBindingStop is NOT
  // called within an on-going Fat operation, so we should
  // take responsibility to cleanup and free the volume.
  // Otherwise, the DriverBindingStop is called within an on-going
  // Fat operation, we shouldn't check reference, so just let outer
  // FatCleanupVolume do the task.
  //
  if (LockedByMe) {
    FatCleanupVolume (Volume, NULL, EFI_SUCCESS);
    FatReleaseLock ();
  }

  return EFI_SUCCESS;
}

EFI_STATUS
FatOpenDevice (
  IN OUT FAT_VOLUME           *Volume
  )
/*++

Routine Description:
 
  Detects FAT file system on Disk and set relevant fields of Volume
  
Arguments:
 
  Volume                - The volume structure.

Returns:

  EFI_SUCCESS           - The Fat File System is detected successfully
  EFI_UNSUPPORTED       - The volume is not FAT file system.
  EFI_VOLUME_CORRUPTED  - The volume is corrupted.
  
--*/
{
  EFI_STATUS            Status;
  UINT32                BlockSize;
  UINT32                DirtyMask;
  EFI_DISK_IO_PROTOCOL  *DiskIo;
  FAT_BOOT_SECTOR       FatBs;
  FAT_VOLUME_TYPE       FatType;
  UINTN                 RootDirSectors;
  UINTN                 RootSize;
  UINTN                 FatLba;
  UINTN                 RootLba;
  UINTN                 FirstClusterLba;
  UINTN                 Sectors;
  UINTN                 SectorsPerFat;
  UINT8                 SectorsPerClusterAlignment;
  UINT8                 BlockAlignment;

  //
  // Read the FAT_BOOT_SECTOR BPB info
  // This is the only part of FAT code that uses parent DiskIo,
  // Others use FatDiskIo which utilizes a Cache.
  //
  DiskIo  = Volume->DiskIo;
  Status  = DiskIo->ReadDisk (DiskIo, Volume->MediaId, 0, sizeof (FatBs), &FatBs);

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_INIT, "FatOpenDevice: read of part_lba failed %r\n", Status));
    return Status;
  }

  FatType = FatUndefined;

  //
  // Use LargeSectors if Sectors is 0
  //
  Sectors = FatBs.FatBsb.Sectors;
  if (Sectors == 0) {
    Sectors = FatBs.FatBsb.LargeSectors;
  }

  SectorsPerFat = FatBs.FatBsb.SectorsPerFat;
  if (SectorsPerFat == 0) {
    SectorsPerFat = FatBs.FatBse.Fat32Bse.LargeSectorsPerFat;
    FatType       = FAT32;
  }
  //
  // Is boot sector a fat sector?
  // (Note that so far we only know if the sector is FAT32 or not, we don't
  // know if the sector is Fat16 or Fat12 until later when we can compute
  // the volume size)
  //
  if (FatBs.FatBsb.ReservedSectors == 0 || FatBs.FatBsb.NumFats == 0 || Sectors == 0) {
    return EFI_UNSUPPORTED;
  }

  if ((FatBs.FatBsb.SectorSize & (FatBs.FatBsb.SectorSize - 1)) != 0) {
    return EFI_UNSUPPORTED;
  }

  BlockAlignment = Log2 (FatBs.FatBsb.SectorSize);
  if (BlockAlignment > MAX_BLOCK_ALIGNMENT || BlockAlignment < MIN_BLOCK_ALIGNMENT) {
    return EFI_UNSUPPORTED;
  }

  if ((FatBs.FatBsb.SectorsPerCluster & (FatBs.FatBsb.SectorsPerCluster - 1)) != 0) {
    return EFI_UNSUPPORTED;
  }

  SectorsPerClusterAlignment = Log2 (FatBs.FatBsb.SectorsPerCluster);
  if (SectorsPerClusterAlignment > MAX_SECTORS_PER_CLUSTER_ALIGNMENT) {
    return EFI_UNSUPPORTED;
  }

  if (FatBs.FatBsb.Media <= 0xf7 &&
      FatBs.FatBsb.Media != 0xf0 &&
      FatBs.FatBsb.Media != 0x00 &&
      FatBs.FatBsb.Media != 0x01
      ) {
    return EFI_UNSUPPORTED;
  }
  //
  // Initialize fields the volume information for this FatType
  //
  if (FatType != FAT32) {
    if (FatBs.FatBsb.RootEntries == 0) {
      return EFI_UNSUPPORTED;
    }
    //
    // Unpack fat12, fat16 info
    //
    Volume->RootEntries = FatBs.FatBsb.RootEntries;
  } else {
    //
    // If this is fat32, refuse to mount mirror-disabled volumes
    //
    if ((SectorsPerFat == 0 || FatBs.FatBse.Fat32Bse.FsVersion != 0) || (FatBs.FatBse.Fat32Bse.ExtendedFlags & 0x80)) {
      return EFI_UNSUPPORTED;
    }
    //
    // Unpack fat32 info
    //
    Volume->RootCluster = FatBs.FatBse.Fat32Bse.RootDirFirstCluster;
  }

  Volume->NumFats           = FatBs.FatBsb.NumFats;
  //
  // Compute some fat locations
  //
  BlockSize                 = FatBs.FatBsb.SectorSize;
  RootDirSectors            = ((Volume->RootEntries * sizeof (FAT_DIRECTORY_ENTRY)) + (BlockSize - 1)) / BlockSize;
  RootSize                  = RootDirSectors * BlockSize;

  FatLba                    = FatBs.FatBsb.ReservedSectors;
  RootLba                   = FatBs.FatBsb.NumFats * SectorsPerFat + FatLba;
  FirstClusterLba           = RootLba + RootDirSectors;

  Volume->FatPos            = FatLba * BlockSize;
  Volume->FatSize           = SectorsPerFat * BlockSize;

  Volume->VolumeSize        = LShiftU64 (Sectors, BlockAlignment);
  Volume->RootPos           = LShiftU64 (RootLba, BlockAlignment);
  Volume->FirstClusterPos   = LShiftU64 (FirstClusterLba, BlockAlignment);
  Volume->MaxCluster        = (Sectors - FirstClusterLba) >> SectorsPerClusterAlignment;
  Volume->ClusterAlignment  = BlockAlignment + SectorsPerClusterAlignment;
  Volume->ClusterSize       = (UINTN)1 << (Volume->ClusterAlignment);

  //
  // If this is not a fat32, determine if it's a fat16 or fat12
  //
  if (FatType != FAT32) {
    if (Volume->MaxCluster >= FAT_MAX_FAT16_CLUSTER) {
      return EFI_VOLUME_CORRUPTED;
    }

    FatType = Volume->MaxCluster < FAT_MAX_FAT12_CLUSTER ? FAT12 : FAT16;
    //
    // fat12 & fat16 fat-entries are 2 bytes
    //
    Volume->FatEntrySize = sizeof (UINT16);
    DirtyMask            = FAT16_DIRTY_MASK;
  } else {
    if (Volume->MaxCluster < FAT_MAX_FAT16_CLUSTER) {
      return EFI_VOLUME_CORRUPTED;
    }
    //
    // fat32 fat-entries are 4 bytes
    //
    Volume->FatEntrySize = sizeof (UINT32);
    DirtyMask            = FAT32_DIRTY_MASK;
  }
  //
  // Get the DirtyValue and NotDirtyValue
  // We should keep the initial value as the NotDirtyValue
  // in case the volume is dirty already
  //
  if (FatType != FAT12) {
    Status = FatAccessVolumeDirty (Volume, READ_DISK, &Volume->NotDirtyValue);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Volume->DirtyValue = Volume->NotDirtyValue & DirtyMask;
    Volume->FatDirty   = (BOOLEAN) (Volume->DirtyValue == Volume->NotDirtyValue);
  }
  //
  // If present, read the fat hint info
  //
  if (FatType == FAT32) {
    Volume->FreeInfoPos = FatBs.FatBse.Fat32Bse.FsInfoSector * BlockSize;
    if (FatBs.FatBse.Fat32Bse.FsInfoSector != 0) {
      FatDiskIo (Volume, READ_DISK, Volume->FreeInfoPos, sizeof (FAT_INFO_SECTOR), &Volume->FatInfoSector);
      if (Volume->FatInfoSector.Signature == FAT_INFO_SIGNATURE &&
          Volume->FatInfoSector.InfoBeginSignature == FAT_INFO_BEGIN_SIGNATURE &&
          Volume->FatInfoSector.InfoEndSignature == FAT_INFO_END_SIGNATURE &&
          Volume->FatInfoSector.FreeInfo.ClusterCount <= Volume->MaxCluster
          ) {
        Volume->FreeInfoValid = TRUE;
      }
    }
  }
  //
  // Just make up a FreeInfo.NextCluster for use by allocate cluster
  //
  if (FAT_MIN_CLUSTER > Volume->FatInfoSector.FreeInfo.NextCluster ||
     Volume->FatInfoSector.FreeInfo.NextCluster > Volume->MaxCluster + 1
     ) {
    Volume->FatInfoSector.FreeInfo.NextCluster = FAT_MIN_CLUSTER;
  }
  //
  // We are now defining FAT Type
  //
  Volume->FatType = FatType;
  ASSERT (FatType != FatUndefined);

  return EFI_SUCCESS;
}
