/*++

Copyright (c) 2006, Intel Corporation
All rights reserved. This program and the accompanying materials
are licensed and made available under the terms and conditions of the Software 
License Agreement which accompanies this distribution.


Module Name:

  ComponentName.c

Abstract:

--*/

#include "Fat.h"

//
// EFI Component Name Functions
//
EFI_STATUS
EFIAPI
FatComponentNameGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **DriverName
  );

EFI_STATUS
EFIAPI
FatComponentNameGetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL                 *This,
  IN  EFI_HANDLE                                  ControllerHandle,
  IN  EFI_HANDLE                                  ChildHandle  OPTIONAL,
  IN  CHAR8                                       *Language,
  OUT CHAR16                                      **ControllerName
  );

//
// EFI Component Name Protocol
//
EFI_COMPONENT_NAME_PROTOCOL     gFatComponentName = {
  FatComponentNameGetDriverName,
  FatComponentNameGetControllerName,
  "eng"
};

static EFI_UNICODE_STRING_TABLE mFatDriverNameTable[] = {
  {
    "eng",
    L"FAT File System Driver"
  },
  {
    NULL,
    NULL
  }
};

static EFI_UNICODE_STRING_TABLE mFatControllerNameTable[] = {
  {
    "eng",
    L"FAT File System"
  },
  {
    NULL,
    NULL
  }
};

EFI_STATUS
EFIAPI
FatComponentNameGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **DriverName
  )
/*++

Routine Description:

  Retrieves a Unicode string that is the user readable name of the EFI Driver.

Arguments:

  This                  - A pointer to the EFI_COMPONENT_NAME_PROTOCOL instance.
  Language              - A pointer to a three character ISO 639-2 language identifier.
                          This is the language of the driver name that that the caller 
                          is requesting, and it must match one of the languages specified
                          in SupportedLanguages.  The number of languages supported by a 
                          driver is up to the driver writer.
  DriverName            - A pointer to the Unicode string to return.  This Unicode string
                          is the name of the driver specified by This in the language 
                          specified by Language.

Returns:

  EFI_SUCCESS           - The Unicode string for the Driver specified by This
                          and the language specified by Language was returned 
                          in DriverName.
  EFI_INVALID_PARAMETER - Language is NULL.
  EFI_INVALID_PARAMETER - DriverName is NULL.
  EFI_UNSUPPORTED       - The driver specified by This does not support the 
                          language specified by Language.

--*/
{
  return EfiLibLookupUnicodeString (
           Language,
           gFatComponentName.SupportedLanguages,
           mFatDriverNameTable,
           DriverName
           );
}

EFI_STATUS
EFIAPI
FatComponentNameGetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL    *This,
  IN  EFI_HANDLE                     ControllerHandle,
  IN  EFI_HANDLE                     ChildHandle OPTIONAL,
  IN  CHAR8                          *Language,
  OUT CHAR16                         **ControllerName
  )
/*++

Routine Description:

  Retrieves a Unicode string that is the user readable name of the controller
  that is being managed by an EFI Driver.

Arguments:
  
  This                  - A pointer to the EFI_COMPONENT_NAME_PROTOCOL instance.
  ControllerHandle      - The handle of a controller that the driver specified by 
                          This is managing.  This handle specifies the controller 
                          whose name is to be returned.
  ChildHandle           - The handle of the child controller to retrieve the name 
                          of.  This is an optional parameter that may be NULL.  It 
                          will be NULL for device drivers.  It will also be NULL 
                          for a bus drivers that wish to retrieve the name of the 
                          bus controller.  It will not be NULL for a bus driver 
                          that wishes to retrieve the name of a child controller.
  Language              - A pointer to a three character ISO 639-2 language 
                          identifier.  This is the language of the controller name 
                          that that the caller is requesting, and it must match one
                          of the languages specified in SupportedLanguages.  The 
                          number of languages supported by a driver is up to the 
                          driver writer.
  ControllerName        - A pointer to the Unicode string to return.  This Unicode
                          string is the name of the controller specified by 
                          ControllerHandle and ChildHandle in the language specified
                          by Language from the point of view of the driver specified
                          by This. 

Returns:

  EFI_SUCCESS           - The Unicode string for the user readable name in the 
                          language specified by Language for the driver 
                          specified by This was returned in DriverName.
  EFI_INVALID_PARAMETER - ControllerHandle is not a valid EFI_HANDLE.
  EFI_INVALID_PARAMETER - ChildHandle is not NULL and it is not a valid 
                          EFI_HANDLE.
  EFI_INVALID_PARAMETER - Language is NULL.
  EFI_INVALID_PARAMETER - ControllerName is NULL.
  EFI_UNSUPPORTED       - The driver specified by This is not currently managing 
                          the controller specified by ControllerHandle and 
                          ChildHandle.
  EFI_UNSUPPORTED       - The driver specified by This does not support the 
                          language specified by Language.

--*/
{
  EFI_STATUS  Status;
  //
  // This is a device driver, so ChildHandle must be NULL.
  //
  if (ChildHandle != NULL) {
    return EFI_UNSUPPORTED;
  }

  //
  // Make sure this driver is currently managing ControllHandle
  //
  Status = EfiTestManagedDevice (
             ControllerHandle,
             gFatDriverBinding.DriverBindingHandle,
             &gEfiDiskIoProtocolGuid
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EfiLibLookupUnicodeString (
           Language,
           gFatComponentName.SupportedLanguages,
           mFatControllerNameTable,
           ControllerName
           );
}
