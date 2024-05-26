#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
#pragma warning ( disable : 4995 )
#include <stdlib.h>
#include <ntstrsafe.h>
#include "DetectZ.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

PFLT_FILTER gFilterHandle = NULL;

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, InstanceSetup)
#pragma alloc_text(PAGE, CleanupVolumeContext)
#pragma alloc_text(PAGE, InstanceQueryTeardown)
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(INIT, ReadDriverParameters)
#pragma alloc_text(PAGE, FilterUnload)
#endif

//
//  Operation we currently care about.
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    { IRP_MJ_CREATE,
      0,
      DetectZPreCreate,
      DetectZPostCreate },

	{ IRP_MJ_CLOSE,
      0,
      DetectZPreClose,
      NULL },

	{ IRP_MJ_CLEANUP,
      0,
      DetectZPreCleanup,
      NULL },

	{ IRP_MJ_READ,
      0,
	  DetectZPreRead,
      NULL },

    { IRP_MJ_WRITE,
      0,
      DetectZPreWrite,
      NULL },

    { IRP_MJ_SET_INFORMATION,
      0,
      DetectZPreSetInfo,
      NULL },

    { IRP_MJ_OPERATION_END }
};

//
//  Context definitions we currently care about.  Note that the system will
//  create a lookAside list for the volume context because an explicit size
//  of the context is specified.
//

CONST FLT_CONTEXT_REGISTRATION ContextNotifications[] = {

     { FLT_VOLUME_CONTEXT,
       0,
       CleanupVolumeContext,
       sizeof(VOLUME_CONTEXT),
       CONTEXT_TAG },
       
     { FLT_STREAMHANDLE_CONTEXT,        //ContextType
       0,                               //Flags
       CleanupAccessData,				//ContextCleanupCallback
       sizeof(FILE_ACCESS_DATA),     	//Size
       CONTEXT_TAG },     				//PoolTag

     { FLT_CONTEXT_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof( FLT_REGISTRATION ),         //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    ContextNotifications,               //  Context
    Callbacks,                          //  Operation callbacks

    FilterUnload,                       //  MiniFilterUnload

    InstanceSetup,                      //  InstanceSetup
    InstanceQueryTeardown,              //  InstanceQueryTeardown
    NULL,                               //  InstanceTeardownStart
    NULL,                               //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};

/*************************************************************************
    Debug tracing information
*************************************************************************/

//
//  Definitions to display log messages.  The registry DWORD entry:
//  "hklm\system\CurrentControlSet\Services\Swapbuffers\DebugFlags" defines
//  the default state of these logging flags
//

#define LOGFL_ERRORS    0x00000001  // if set, display error messages
#define LOGFL_READ      0x00000002  // if set, display READ operation info
#define LOGFL_WRITE     0x00000004  // if set, display WRITE operation info
#define LOGFL_DIRCTRL   0x00000008  // if set, display DIRCTRL operation info
#define LOGFL_VOLCTX    0x00000010  // if set, display VOLCTX operation info

ULONG LoggingFlags = 0;             // all disabled by default

#define LOG_PRINT( _logFlag, _string )                              \
    (FlagOn(LoggingFlags,(_logFlag)) ?                              \
        DbgPrint _string  :                                         \
        ((void)0))

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//                      Routines
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////


NTSTATUS
InstanceSetup (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_SETUP_FLAGS Flags,
    __in DEVICE_TYPE VolumeDeviceType,
    __in FLT_FILESYSTEM_TYPE VolumeFilesystemType
    )
/*++

Routine Description:

    This routine is called whenever a new instance is created on a volume.

    By default we want to attach to all volumes.  This routine will try and
    get a "DOS" name for the given volume.  If it can't, it will try and
    get the "NT" name for the volume (which is what happens on network
    volumes).  If a name is retrieved a volume context will be created with
    that name.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Flags describing the reason for this attach request.

Return Value:

    STATUS_SUCCESS - attach
    STATUS_FLT_DO_NOT_ATTACH - do not attach

--*/
{

    PDEVICE_OBJECT devObj = NULL;
    PVOLUME_CONTEXT ctx = NULL;
    NTSTATUS status;
    ULONG retLen;
    PUNICODE_STRING workingName;
    USHORT size;
    UCHAR volPropBuffer[sizeof(FLT_VOLUME_PROPERTIES)+512];
    PFLT_VOLUME_PROPERTIES volProp = (PFLT_VOLUME_PROPERTIES)volPropBuffer;

    PAGED_CODE();

    UNREFERENCED_PARAMETER( Flags );
    UNREFERENCED_PARAMETER( VolumeDeviceType );
    UNREFERENCED_PARAMETER( VolumeFilesystemType );

    //�@�R���e�L�X�g�̃������m��
    //  Allocate a volume context structure.
    //

    status = FltAllocateContext( FltObjects->Filter,
                                 FLT_VOLUME_CONTEXT,
                                 sizeof(VOLUME_CONTEXT),
                                 NonPagedPool,
                                 &ctx );

    if (NT_SUCCESS(status)) {

	    //	�{�����[���̃v���p�e�B�擾
	    //  Always get the volume properties, so I can get a sector size
	    //

	    status = FltGetVolumeProperties( FltObjects->Volume,
	                                     volProp,
	                                     sizeof(volPropBuffer),
	                                     &retLen );
		
    }

    if (NT_SUCCESS(status)) {

	    //
	    //  Save the sector size in the context for later use.  Note that
	    //  we will pick a minimum sector size if a sector size is not
	    //  specified.
	    //

	    ASSERT((volProp->SectorSize == 0) || (volProp->SectorSize >= MIN_SECTOR_SIZE));

	    ctx->SectorSize = max(volProp->SectorSize,MIN_SECTOR_SIZE);

	    //
	    //  Init the buffer field (which may be allocated later).
	    //

	    ctx->Name.Buffer = NULL;

	    //  �Ώۃf�B�X�N�̃f�o�C�X�I�u�W�F�N�g�擾
	    //  Get the storage device object we want a name for.
	    //

	    status = FltGetDiskDeviceObject( FltObjects->Volume, &devObj );
	    

    }

    if (NT_SUCCESS(status)) {

	    //	
	    //  Try and get the DOS name.  If it succeeds we will have
	    //  an allocated name buffer.  If not, it will be NULL
	    //

	    status = RtlVolumeDeviceToDosName( devObj, &ctx->Name );

        if (NT_SUCCESS(status)) {

		    //  �R���e�L�X�g��ݒ肷��
		    //  Set the context
		    //

		    status = FltSetVolumeContext( FltObjects->Volume,
		                                  FLT_SET_CONTEXT_KEEP_IF_EXISTS,
		                                  ctx,
		                                  NULL );

		    //
		    //  Log debug info
		    //

		    LOG_PRINT( LOGFL_VOLCTX,
		               ("SwapBuffers!InstanceSetup:                  Real SectSize=0x%04x, Used SectSize=0x%04x, Name=\"%wZ\"\n",
		                volProp->SectorSize,
		                ctx->SectorSize,
		                &ctx->Name) );

		    //
		    //  It is OK for the context to already be defined.
		    //

		    if (status == STATUS_FLT_CONTEXT_ALREADY_DEFINED) {

		        status = STATUS_SUCCESS;
		    }
        	
        }
        else {
        	// Dos�����擾�ł��Ȃ��ꍇ�A�l�b�g���[�N�̂��߃A�^�b�`���Ȃ�
        	status = STATUS_FLT_DO_NOT_ATTACH;
        }


        //
	    //  Remove the reference added to the device object by
	    //  FltGetDiskDeviceObject.
	    //

	    if (devObj) {

	        ObDereferenceObject( devObj );
		}
	}

    //
    //  Always release the context.  If the set failed, it will free the
    //  context.  If not, it will remove the reference added by the set.
    //  Note that the name buffer in the ctx will get freed by the context
    //  cleanup routine.
    //

    if (ctx) {

        FltReleaseContext( ctx );
    }
    return status;
}


VOID
CleanupVolumeContext(
    __in PFLT_CONTEXT Context,
    __in FLT_CONTEXT_TYPE ContextType
    )
/*++

Routine Description:

    The given context is being freed.
    Free the allocated name buffer if there one.

Arguments:

    Context - The context being freed

    ContextType - The type of context this is

Return Value:

    None

--*/
{
    PVOLUME_CONTEXT ctx = Context;

    PAGED_CODE();

    UNREFERENCED_PARAMETER( ContextType );

    ASSERT(ContextType == FLT_VOLUME_CONTEXT);

    if (ctx->Name.Buffer != NULL) {

        ExFreePool(ctx->Name.Buffer);
        ctx->Name.Buffer = NULL;
    }
}



VOID
CleanupAccessData(
    __in PFLT_CONTEXT Context,
    __in FLT_CONTEXT_TYPE ContextType
    )
/*++

Routine Description:

    The given context is being freed.
    Free the allocated name buffer if there one.

Arguments:

    Context - The context being freed

    ContextType - The type of context this is

Return Value:

    None

--*/
{
    PFILE_ACCESS_DATA pFileAccessData = Context;

    PAGED_CODE();

    UNREFERENCED_PARAMETER( ContextType );

    ASSERT(ContextType == FLT_STREAMHANDLE_CONTEXT);

	// �m�ۂ����������̉��
    if (pFileAccessData->FullPath.Buffer != NULL) {

        ExFreePool(pFileAccessData->FullPath.Buffer);
        pFileAccessData->FullPath.Buffer = NULL;
    }
    
    if (pFileAccessData->ProcessName != NULL) {
        ExFreePool(pFileAccessData->ProcessName);
        pFileAccessData->ProcessName = NULL;
    }
}

NTSTATUS
InstanceQueryTeardown (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This is called when an instance is being manually deleted by a
    call to FltDetachVolume or FilterDetach.  We always return it is OK to
    detach.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Indicating where this detach request came from.

Return Value:

    Always succeed.

--*/
{
    PAGED_CODE();
    
	KdPrint(("DetectZ : InstanceQueryTeardown Enter\n"));

    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );
    
	KdPrint(("DetectZ : InstanceQueryTeardown Exit\n"));

    return STATUS_SUCCESS;
}

VOID
ReadDriverParameters (
    __in PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This routine tries to read the driver-specific parameters from
    the registry.  These values will be found in the registry location
    indicated by the RegistryPath passed in.

Arguments:

    RegistryPath - the path key passed to the driver during driver entry.

Return Value:

    None.

--*/
{
    OBJECT_ATTRIBUTES attributes;
    HANDLE driverRegKey;
    NTSTATUS status;
    ULONG resultLength;
    UNICODE_STRING valueName;
    UCHAR buffer[sizeof( KEY_VALUE_PARTIAL_INFORMATION ) + sizeof( LONG )];

    //
    //  If this value is not zero then somebody has already explicitly set it
    //  so don't override those settings.
    //

    if (0 == LoggingFlags) {

        //
        //  Open the desired registry key
        //

        InitializeObjectAttributes( &attributes,
                                    RegistryPath,
                                    OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                                    NULL,
                                    NULL );

        status = ZwOpenKey( &driverRegKey,
                            KEY_READ,
                            &attributes );

        if (!NT_SUCCESS( status )) {

            return;
        }

        //
        // Read the given value from the registry.
        //

        RtlInitUnicodeString( &valueName, L"DebugFlags" );

        status = ZwQueryValueKey( driverRegKey,
                                  &valueName,
                                  KeyValuePartialInformation,
                                  buffer,
                                  sizeof(buffer),
                                  &resultLength );

        if (NT_SUCCESS( status )) {

            LoggingFlags = *((PULONG) &(((PKEY_VALUE_PARTIAL_INFORMATION)buffer)->Data));
        }

        //
        //  Close the registry entry
        //

        ZwClose(driverRegKey);
    }
}

void UTIL_UnicodeStringUpper(WCHAR *UniStr)
{
#ifdef FREE_LOG
	DbgPrint("UTIL_UnicodeStringUpper(%8.8lx)\n", (ULONG)UniStr);
#endif

	WCHAR *CurrentPtr = UniStr;

	while(TRUE){
		if (CurrentPtr[0] == L'\0'){
			break;
		}else{
			if ((CurrentPtr[0] >= L'a') && (CurrentPtr[0] <= L'z')){
				USHORT Dif = L'z' - CurrentPtr[0];
				CurrentPtr[0] = L'Z' - Dif;
			}
		}
		CurrentPtr++;
	}
}


/*************************************************************************
    Initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This is the initialization routine.  This registers with FltMgr and
    initializes all global data structures.

Arguments:

    DriverObject - Pointer to driver object created by the system to
        represent this driver.

    RegistryPath - Unicode string identifying where the parameters for this
        driver are located in the registry.

Return Value:

    Status of the operation

--*/
{
	
    NTSTATUS status;
	KdPrint(("DetectZ : DriverEntry Enter\n"));

    //
    //  Get debug trace flags
    //

    ReadDriverParameters( RegistryPath );


    //�@FltMgr�ɖ{���W���[����o�^
    //  Register with FltMgr
    //
    status = FltRegisterFilter( DriverObject,
                                &FilterRegistration,
                                &gFilterHandle );

    if (NT_SUCCESS( status )) {

        //�@�t�B���^�J�n
        //  Start filtering i/o
        //

        status = FltStartFiltering( gFilterHandle );
        
        if( NT_SUCCESS(status)){
        	// �[���f�C�U���Ώېݒ�擾
        	// �ݒ肪�����Ă��Ď����邽�߁A�G���[�l�̓`�F�b�N���Ȃ�
        	InitializeListHead(&gDetectList.ListEntry);
        	GetAndSetDetectSetting();
        }
    }
    
 	if (NT_SUCCESS( status ) == FALSE) {
 		// �����ꂩ�̏����Ɏ��s
        FltUnregisterFilter( gFilterHandle );
    }
 
	KdPrint(("DetectZ : DriverEntry Exit\n"));

    return status;
}


NTSTATUS
FilterUnload (
    __in FLT_FILTER_UNLOAD_FLAGS Flags
    )
/*++

Routine Description:

    Called when this mini-filter is about to be unloaded.  We unregister
    from the FltMgr and then return it is OK to unload

Arguments:

    Flags - Indicating if this is a mandatory unload.

Return Value:

    Returns the final status of this operation.

--*/
{
    PAGED_CODE();
    
	KdPrint(("DetectZ : FilterUnload Enter\n"));

    UNREFERENCED_PARAMETER( Flags );

	if(gFilterHandle != NULL){
	    //
	    //  Unregister from FLT mgr
	    //

	    FltUnregisterFilter( gFilterHandle );
	}


	KdPrint(("DetectZ : FilterUnload Exit\n"));

    return STATUS_SUCCESS;
}


/*************************************************************************
    MiniFilter callback routines.
*************************************************************************/

FLT_PREOP_CALLBACK_STATUS
DetectZPreCreate(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine is called on Pre-CREATE operation.
    It doesn't do anything for now.

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - Receives the context that will be passed to the
        post-operation callback.

Return Value:

    FLT_PREOP_SUCCESS_WITH_CALLBACK - we want a postOpeation callback
    
--*/
{
	// ���łł͉������Ȃ�

    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );
    
    
    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
    
}



FLT_POSTOP_CALLBACK_STATUS
DetectZPostCreate(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PVOID CompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
/*++

Routine Description:

	This routine is called on POST-CREATE operation.
	It aquires file access information, then store it
    on the context which FltMgr Manage.

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The completion context set in the pre-operation routine.

    Flags - Denotes whether the completion is successful or is being drained.

Return Value:

    FLT_POSTOP_FINISHED_PROCESSING

--*/
{
    NTSTATUS 			status;
    PFILE_ACCESS_DATA 	AccessData = NULL;

    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( CompletionContext );
    UNREFERENCED_PARAMETER( Flags );

    //
    //  This system won't draining an operation with swapped buffers, verify
    //  the draining flag is not set.
    //

    ASSERT(!FlagOn(Flags, FLTFL_POST_OPERATION_DRAINING));

	// �R���e�L�X�g�p�̃������m��
	status = FltAllocateContext(
				FltObjects->Filter,
				FLT_STREAMHANDLE_CONTEXT,
				sizeof(FILE_ACCESS_DATA),
				NonPagedPool,
				&AccessData);
	
	if( NT_SUCCESS(status)){
		// �t�@�C�����擾
		status = MakeFileAccessData( Data, FltObjects, (PFLT_CONTEXT)AccessData);
		
		if( NT_SUCCESS(status)){
			// �A�N�Z�X������擾
			status = GetAccessCondition( AccessData);
			if( NT_SUCCESS(status)){
				// �R���e�L�X�g�ɏ���o�^
				status = FltSetStreamHandleContext(
									FltObjects->Instance,
									FltObjects->FileObject,
		    						FLT_SET_CONTEXT_REPLACE_IF_EXISTS,
		    						AccessData,
		    						NULL);
			}
		}
		// �R���e�L�X�g�����[�X
    	FltReleaseContext( AccessData );
	}
	else {
		status = STATUS_INSUFFICIENT_RESOURCES;
	}
	
	
	return FLT_POSTOP_FINISHED_PROCESSING;
}



FLT_PREOP_CALLBACK_STATUS
DetectZPreClose(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine is called on Pre-CLOSE operation.
    It doesn't do anything for now.

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - Receives the context that will be passed to the
        post-operation callback.

Return Value:

    FLT_PREOP_SUCCESS_NO_CALLBACK - we don't want a postOperation callback

--*/
{

    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );
    
   
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
    
}

FLT_PREOP_CALLBACK_STATUS
DetectZPreCleanup(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine is called on Pre-CLEANUP operation.
    It doesn't do anything for now.

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - Receives the context that will be passed to the
        post-operation callback.

Return Value:

    FLT_PREOP_SUCCESS_NO_CALLBACK - we don't want a postOperation callback

--*/
{
	// ���łł͉������Ȃ�

    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );

   
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
    
}

FLT_PREOP_CALLBACK_STATUS
DetectZPreRead(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine is called on Pre-READ operation.
    It sets READ flag to the Context.
    If file acess matches the Zero-Day Attack condition,
    it proceeds notification process.

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - Receives the context that will be passed to the
        post-operation callback.

Return Value:

    FLT_PREOP_SUCCESS_WITH_CALLBACK - we want a postOpeation callback
    FLT_PREOP_SUCCESS_NO_CALLBACK - we don't want a postOperation callback

--*/
{
	NTSTATUS			status;
	PFILE_ACCESS_DATA	pFileAccessData = NULL;

	KdPrint(("DetectZ : DetectZPreRead Enter\n"));

    UNREFERENCED_PARAMETER( CompletionContext );
    
    // �����L���H
    if(Data != NULL && FltObjects != NULL){
    	// �A�N�Z�X���擾
	    status = FltGetStreamHandleContext(
									FltObjects->Instance,
									FltObjects->FileObject,
									&pFileAccessData);
	    if(NT_SUCCESS(status)){
	        // �������݃t���O�ݒ�
	        pFileAccessData->AccessRecord |= READ_OPERATION;
	        
	        // �[���f�C�U���H
	        if(IsZeroAttack( pFileAccessData, READ_OPERATION) == TRUE){
	        	// �[���f�C�U���ʒm
	        	status = NotifyAttackData( Data, pFileAccessData, READ_OPERATION);
	        }
	        
	        // �A�N�Z�X���㏑��
	        FltSetFileContext(
						FltObjects->Instance,
						FltObjects->FileObject,
						FLT_SET_CONTEXT_REPLACE_IF_EXISTS,
	        			pFileAccessData,
	        			NULL); 
	        
	    	// Set�������̃��t�@�����X�J�E���g�_�E��
	    	FltReleaseContext(pFileAccessData);
	        
	    }
	}
    
	KdPrint(("DetectZ : DetectZPreRead Exit\n"));
    
    return FLT_PREOP_SUCCESS_NO_CALLBACK;

    
}


FLT_PREOP_CALLBACK_STATUS
DetectZPreWrite(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine is called on Pre-WRITE operation.
    It sets WRITE flag to the Context.
    If file acess matches the Zero-Day Attack condition,
    it proceeds notification process.

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - Receives the context that will be passed to the
        post-operation callback.

Return Value:

    FLT_PREOP_SUCCESS_WITH_CALLBACK - we want a postOpeation callback
    FLT_PREOP_SUCCESS_NO_CALLBACK - we don't want a postOperation callback

--*/
{
	NTSTATUS			status;
	PFILE_ACCESS_DATA	pFileAccessData = NULL;

	KdPrint(("DetectZ : DetectZPreWrite Enter\n"));

    UNREFERENCED_PARAMETER( CompletionContext );
    
    // �����L���H
    if( Data != NULL && FltObjects != NULL){
    	// �A�N�Z�X���擾
	    status = FltGetStreamHandleContext(
							FltObjects->Instance,
							FltObjects->FileObject,
	    					&pFileAccessData);
	    if(NT_SUCCESS(status)){
	        // �������݃t���O�ݒ�
	        pFileAccessData->AccessRecord |= WRITE_OPERATION;
	        
	        // �[���f�C�U���H
	        if(IsZeroAttack( pFileAccessData, WRITE_OPERATION) == TRUE){
	        	// �[���f�C�U���ʒm
	        	status = NotifyAttackData( Data, pFileAccessData, WRITE_OPERATION);
	        }
	        
	        // �A�N�Z�X���㏑��
	        FltSetFileContext(
						FltObjects->Instance,
						FltObjects->FileObject,
	        			FLT_SET_CONTEXT_REPLACE_IF_EXISTS,
	        			pFileAccessData,
	        			NULL); 
	        
	        // �R���e�L�X�g�����[�X
	        FltReleaseContext(pFileAccessData);
	        
	    }
	}
    
	KdPrint(("DetectZ : DetectZPreWrite Exit\n"));
    
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
    
}


FLT_PREOP_CALLBACK_STATUS
DetectZPreSetInfo(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine is called on Pre-SET_INFORMATION operation.
    It doesn't do anything for now.

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - Receives the context that will be passed to the
        post-operation callback.

Return Value:

    FLT_PREOP_SUCCESS_WITH_CALLBACK - we want a postOpeation callback
    FLT_PREOP_SUCCESS_NO_CALLBACK - we don't want a postOperation callback

--*/
{
	// ���łł͉������Ȃ�
	KdPrint(("DetectZ : DetectZPreSetInfo Enter\n"));

    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );
  	KdPrint(("DetectZ : DetectZPreSetInfo Exit\n"));
  
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
    
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
//		FileAccessData
//////////////////////////////////////////////////////////////////////////////////////////////////////

NTSTATUS
MakeFileAccessData(
    __in 	PFLT_CALLBACK_DATA		Data,
    __in PCFLT_RELATED_OBJECTS 		FltObjects,
    __inout PFLT_CONTEXT			pContext
    )
/*++

Routine Description:

    It acquires the information of the file access that is
    currently processing. For the objects of the context that
    is just a pointer, it allocates memory.
    
Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    pContext - Pointer to the FLT_CONTEXT data structure
        
Return Value:

    STATUS_SUCCESS
    STATUS_INVALID_PARAMETER
    STATUS_UNSUCCESS
    
--*/
{
	NTSTATUS 			status;
	PFILE_ACCESS_DATA	pFileAccessData;
	
	

	// �����͗L���H
    if( Data != NULL && pContext != NULL && FltObjects != NULL){
    	pFileAccessData = (PFILE_ACCESS_DATA)pContext;
    	RtlZeroMemory(pFileAccessData, sizeof(FILE_ACCESS_DATA));
    	
    	// �f�B���N�g�����ۂ����擾
    	status = FltIsDirectory(
    				Data->Iopb->TargetFileObject,
    				Data->Iopb->TargetInstance,
    				&pFileAccessData->IsDirectory);
    	
    	if(NT_SUCCESS(status)){
    		// �v���Z�X���p�̃������m��
    		pFileAccessData->ProcessName = ExAllocatePoolWithTag(NonPagedPool,
											NT_PROCNAMELEN,
											'CORP');
			if(pFileAccessData->ProcessName == NULL){
				// �������m�ۂɎ��s
				status = STATUS_INSUFFICIENT_RESOURCES;
			}
			else{
	    		// �v���Z�X���̎擾
	    		status = GetProcessName(pFileAccessData->ProcessName);
	    	}
    	}
    	if(NT_SUCCESS(status)){
    		// �t�@�C�����̎擾
    		status = ConstructFileName( Data, FltObjects, pFileAccessData);
    		if(NT_SUCCESS(status) == FALSE){
    			// ���s������v���Z�X���p�̃��������������
    			ExFreePool(pFileAccessData->ProcessName);
    			pFileAccessData->ProcessName = NULL;
    		}

    	}
    }
    else {
    	status = STATUS_INVALID_PARAMETER;
    }
    
    return status;
}

NTSTATUS GetProcessName(
	PWCHAR	ProcessName)
{
// �S�̓I�ɕς��̂Ō��f�[�^�폜
	NTSTATUS	Status = STATUS_SUCCESS;
	char		*nameptr;

	// �����͗L���H
	if (ProcessName != NULL){

		// ���Ƀv���Z�X�����擾�ł��Ȃ������ꍇ�A�v���Z�X���i�[�p��������
		// �������ŎQ�Ƃ��Ă����Ȃ��悤�A�^�[�~�l�[�^������
		ProcessName[0] = '\0';
		
		Status = GetProcessFullName(ProcessName, NT_PROCNAMELEN);
	/*	if(!NT_SUCCESS(Status)){
	
			// ���{�ꖢ�Ή��̃v���Z�X���擾
			PEPROCESS	curprc = PsGetCurrentProcess() ;
			nameptr = (PCHAR) curprc + ProcessNameOffset;

			if (nameptr != NULL){
				// �v���Z�X���̑O���[]��t������
				strcpy(ProcessName, "[");
				strcat(ProcessName, nameptr);
				strcat(ProcessName, "]");
				UTIL_AnsiStringUpper(ProcessName);
			} else {
				// �v���Z�X���擾�ł��Ȃ��ꍇ��[unknown]�ɂ���
				strcpy(ProcessName, "[unknown]");
			}
		}
		Status = STATUS_SUCCESS;
	*/
	}else{
		Status = STATUS_INVALID_PARAMETER;
	}
	return Status;
}


NTSTATUS GetProcessFullName(
	PWCHAR	ProcessNameBuffer, 
	ULONG 	BufferLen)
{
	NTSTATUS		Status = STATUS_UNSUCCESSFUL;	// �߂�l���i�[����
	char			*ProcBuf = NULL;				// �v���Z�X���擾�p������
	PPROCESS_BASIC_INFORMATION ProcInfo;			// �v���Z�X���ێ��|�C���^
	ULONG			nReturned;
	PRTL_USER_PROCESS_PARAMETERS UserParams = NULL;	// �v���Z�X����ێ�����p�����[�^�[

	// �����͗L���H
	if(ProcessNameBuffer != NULL){
		// �v���Z�X���擾�p�������m��
		ProcBuf  = (char*)ExAllocatePoolWithTag( PagedPool, sizeof(PROCESS_BASIC_INFORMATION), '61CN') ;
		if (ProcBuf != NULL){
			
			// �v���Z�X���擾
			ZwQueryInformationProcess(
							NtCurrentProcess(),
							ProcessBasicInformation,
							(PVOID)ProcBuf,
							sizeof(PROCESS_BASIC_INFORMATION),
							&nReturned);
			ProcInfo = (PPROCESS_BASIC_INFORMATION)ProcBuf;

			if (ProcInfo){
				if (ProcInfo->PebBaseAddress){
					if (ProcInfo->PebBaseAddress->ProcessParameters){
						// �v���Z�X���͗L��
						UserParams = ProcInfo->PebBaseAddress->ProcessParameters;
					}
				}
			}
		} else {
			// �v���Z�X���擾�p�������m�ێ��s
			Status = STATUS_INSUFFICIENT_RESOURCES;
		}
	} else {
		Status = STATUS_INVALID_PARAMETER;
	}

	if(UserParams != NULL){
		// �擾�����v���Z�X��񂪗L��

		// �v���Z�X���̊J�n�ƏI���̈ʒu�擾
		WCHAR *wBeginPos = NULL;	// ��ԍŌ��'\\'���o������ʒu
		// �Ō�̕����̈ʒu(NULL�͊܂�ł��Ȃ�)
		WCHAR *wLastPos = &UserParams->ImagePathName.Buffer[UserParams->ImagePathName.Length / 2];

		if (!MmIsAddressValid(UserParams->ImagePathName.Buffer)){
		} else {
			// �Ō��\\�̈ʒu��T��
			WCHAR *wProcessFullName = UserParams->ImagePathName.Buffer;
			LONG i;
			for (i = UserParams->ImagePathName.Length / 2 - 1; i >= 0; i --){
				if (wBeginPos == NULL){
					if (UserParams->ImagePathName.Buffer[i] == L'\\'){
						wBeginPos = &UserParams->ImagePathName.Buffer[i];
						wBeginPos ++;	// ���݂̏ꏊ��\\�Ȃ̂ŁA�P�����i�߂�EXE���̐擪�ɂ���
						break;
					}
				}
			}
		}

		if (wBeginPos && wLastPos){
			ULONG CopyLen = (ULONG)wLastPos - (ULONG)wBeginPos;

			if (BufferLen > CopyLen){
				// �v���Z�X���̊i�[
				RtlCopyMemory(ProcessNameBuffer, wBeginPos, CopyLen);	// �����񕔕��̃R�s�[
				ProcessNameBuffer[CopyLen / 2] = L'\0';					// �Ō��NULL
				UTIL_UnicodeStringUpper(ProcessNameBuffer);
				Status = STATUS_SUCCESS;
			}
		}
	}
	if(ProcBuf != NULL){
		ExFreePool(ProcBuf);
	}
	return Status;
}

NTSTATUS ConstructFileName(
	PFLT_CALLBACK_DATA			Data, 
	PCFLT_RELATED_OBJECTS 		FltObjects,
	PFLT_CONTEXT				pContext)
{
	NTSTATUS					status 		= STATUS_SUCCESS;
	PVOLUME_CONTEXT 			ctx 		= NULL;
	PFILE_ACCESS_DATA			pAccessData = NULL;
	PFLT_FILE_NAME_INFORMATION 	pFileName 	= NULL;
	PWCHAR						pStartPtr 	= NULL;
	PWCHAR						pEndPtr 	= NULL;
	BOOLEAN						bContinue;
	int							nCount;
	PWCHAR						pTemp;
	
	
	// �����͗L���H
	if (Data != NULL && FltObjects != NULL && pContext != NULL){
		
		// �h���C�u���̎擾
		status = FltGetVolumeContext( 
					FltObjects->Filter,
					FltObjects->Volume,
					&ctx);
		
        if (NT_SUCCESS(status)) {
        	// �t�@�C�����̎擾
        	status = FltGetFileNameInformation(
        				Data,
        				FLT_FILE_NAME_NORMALIZED,
        				&pFileName);
        	
        	if(NT_SUCCESS(status)){
				// �t�@�C�����̍\�z
				
				// �擾�����t�@�C������"\Device\HarddiskVolume1\Documents and Settings�E�E"�ƂȂ邽�߁A
				// 3�ڂ�\����̃t�@�C�������擾����
				pStartPtr = pFileName->Name.Buffer;
				bContinue = TRUE;
				nCount = 0;
				while(bContinue == TRUE){
					if(*pStartPtr == L'\\'){
						nCount++;
						if(nCount == 3){
							bContinue = FALSE;
							break;
						}
					}
					pStartPtr++;
				}
				
				// �t�@�C�����̖�����":stream1"�����݂���P�[�X�̂��߁A�폜����
				pEndPtr = pStartPtr;
				bContinue = TRUE;
				// �t�@�C�����̍Ō���w��
				pTemp = pFileName->Name.Buffer + (pFileName->Name.Length/sizeof(WCHAR)) -1;
				while(bContinue == TRUE){
					if( (*pEndPtr == L':') || (pEndPtr >= pTemp) ){
						// �F�𔭌��܂��́A�t�@�C���������̕������m�F�����̂ŏI���
						bContinue = FALSE;
					}
					pEndPtr++;
				}
				
				pTemp = ExAllocatePoolWithTag(NonPagedPool,
												(int)( ((char*)pEndPtr - (char*)pStartPtr) + sizeof(WCHAR)),
												'PMET');
				if(pTemp != NULL){
					RtlCopyMemory(pTemp, pStartPtr, (int)((char*)pEndPtr - (char*)pStartPtr));
					pTemp[((char*)pEndPtr - (char*)pStartPtr)/sizeof(WCHAR)] = L'\0';
					UTIL_UnicodeStringUpper(pTemp);
					
					// �t�@�C�����p�̃������m��
					pAccessData = (PFILE_ACCESS_DATA)pContext;
					pAccessData->FullPath.Length = 0;
					pAccessData->FullPath.MaximumLength = pFileName->Name.MaximumLength;
					pAccessData->FullPath.Buffer = ExAllocatePoolWithTag( 
														NonPagedPool,
														pAccessData->FullPath.MaximumLength,
														'TSET');
					if(pAccessData->FullPath.Buffer != NULL){
						// �t���p�X�̍\�z
						RtlCopyUnicodeString( &pAccessData->FullPath, &ctx->Name);
						RtlAppendUnicodeToString( &pAccessData->FullPath, pTemp);
					}
					else {
						// �t���p�X�p�̃������m�ێ��s
						status = STATUS_INSUFFICIENT_RESOURCES;
					}
					ExFreePool(pTemp);
				}
				else {
					// �ꎞ�������m�ێ��s
					status = STATUS_INSUFFICIENT_RESOURCES;
				}
				// �t�@�C�����̃������������[�X
				FltReleaseFileNameInformation( pFileName);
        	}
    		// �擾�����R���e�L�X�g�̃����[�X
			FltReleaseContext( ctx );
        }

	}
	else {
		// �������s��
		status = STATUS_INVALID_PARAMETER;
	}
	
	return status;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//		Zero-day Attack
//////////////////////////////////////////////////////////////////////////////////////////////////////

NTSTATUS	GetAndSetDetectSetting()
{
	NTSTATUS 		status;
	PCHAR			pBuffer = NULL;
	BOOLEAN			bContinue;
	UNICODE_STRING	SettingFile;
	ULONG			ulReadSize;
	ULONG			ulReadOffset;

	
	// �ǂݍ��ݗp�������m��
	pBuffer = ExAllocatePoolWithTag( NonPagedPool,
									READ_DATA_SIZE + 1,
									'DAER');
	if(pBuffer != NULL){
		bContinue = TRUE;
		ulReadSize = READ_DATA_SIZE;
		ulReadOffset = 0;
		RtlInitUnicodeString( &SettingFile, L"\\DosDevices\\C:\\DetectZSetting.txt");
		while(bContinue == TRUE){
			status = ReadFileData( SettingFile,
									&ulReadSize,
									&ulReadOffset,
									pBuffer);
			if( NT_SUCCESS(status)){
				// ����Ώۏ��ݒ�
				status = SetDetectSetting(	pBuffer,
											&ulReadOffset);
			}
			else{
				// �A�N�Z�X���擾���s
				break;
			}
			
			if(ulReadSize < READ_DATA_SIZE || ulReadOffset == 0){
				// �f�[�^�̓ǂݍ��݂܂��͐ݒ�ōŌ�܂Ŋ��������Ɣ��f
				bContinue = FALSE;
			}
			else {
				// ���������A�ݒ�����擾����
				ulReadSize = READ_DATA_SIZE;
			}
		}
		
		ExFreePool(pBuffer);
	}
	else{
		status = STATUS_INSUFFICIENT_RESOURCES;
	}
	return status;
}

NTSTATUS	ReadFileData(
	UNICODE_STRING		FileName,
	PULONG				pulReadSize,
	PULONG				pulReadOffset,
	PVOID				pReadBuffer)
{
	NTSTATUS			status;
	HANDLE				hFile;
	OBJECT_ATTRIBUTES	ObjectAttributes;
	IO_STATUS_BLOCK		IoStatusBlock;
	LARGE_INTEGER 		offset;	
	
	// �����͗L���H
	if( FileName.Buffer != NULL && pulReadSize != NULL && pulReadOffset != NULL && pReadBuffer != NULL){
		// Passive���x���H
		if(KeGetCurrentIrql() == PASSIVE_LEVEL){
			status = STATUS_SUCCESS;
		}
		else{
			// �s����IRQL
			status = STATUS_UNSUCCESSFUL;
		}
	}
	else {
		// ����������
		status = STATUS_INVALID_PARAMETER;
	}
	
	if( NT_SUCCESS(status)){
		InitializeObjectAttributes(
					&ObjectAttributes,
					&FileName,
					OBJ_CASE_INSENSITIVE,
					NULL,
					NULL);

		// �t�@�C�����J��
		status = FltCreateFile(
					gFilterHandle,		// Filter
					NULL,				// Instance OPTIONAL
					&hFile,				// FileHandle
					GENERIC_READ,		// DesiredAccess
					&ObjectAttributes,	// ObjectAttributes
					&IoStatusBlock,		// IoStatusBlock
					NULL,				// AllocationSize OPTIONAL
					0L,					// FileAttributes
					FILE_SHARE_READ,	// ShareAccess
					FILE_OPEN,			// CreateDisposition
					FILE_NON_DIRECTORY_FILE, // CreateOptions
					NULL,				// EaBuffer OPTIONAL
					0L,					// EaLength
					0					// Flags
					);

		if(NT_SUCCESS(status)){

			offset.QuadPart = *pulReadOffset;
			// �t�@�C����ǂݍ���
			status = ZwReadFile( 
						hFile,
						NULL,
						NULL,
						NULL,
						&IoStatusBlock,
						pReadBuffer,
						READ_DATA_SIZE,
						&offset,
						NULL);

			if(NT_SUCCESS(status) == TRUE){
				*pulReadSize = IoStatusBlock.Information;
			}
			else if(status == STATUS_END_OF_FILE){
				// �ǂ݂͂��߂��AEOF�𒴂��Ă���ꍇ�͐����ɂ��A�ǂݍ��񂾃T�C�Y��0�ɂ���
				status = STATUS_SUCCESS;
				*pulReadSize = 0;
			}
			// �t�@�C����ǂݍ���
			FltClose(hFile);
		}
	}
	   	
	return status;
}

NTSTATUS SetDetectSetting(
				PVOID		pReadBuffer,
				PULONG		pulReadOffset)
{
	NTSTATUS	status = STATUS_SUCCESS;
	char		szSearch[] = ",";
	int			nSearchCount;
	PDETECT_SETTING_LIST	pSettingList;
    KIRQL oldIrql;
	PCHAR		pStartPtr;
	PCHAR		pSearchPtr = NULL;
	char		szTemp[_MAX_PATH];
	WCHAR		wcsTemp[_MAX_PATH];
	PLIST_ENTRY	pList;
	
	// �����͗L���H
	if( pReadBuffer != NULL && pulReadOffset!= NULL){
		nSearchCount = 1;
		pStartPtr = (PCHAR)pReadBuffer;
		pSearchPtr = strchr((char*)pReadBuffer, szSearch[0]);
		pSettingList = ExAllocatePoolWithTag(NonPagedPool, sizeof(DETECT_SETTING_LIST), 'TSIL');
		
		while( (pSearchPtr != NULL) && (pSettingList!= NULL) ){
			RtlCopyMemory(szTemp, pStartPtr, (int)((char*)pSearchPtr - (char*)pStartPtr));
			szTemp[pSearchPtr - pStartPtr] = '\0';
			switch(nSearchCount){
				case 1:
					// ���X�g���[���N���A����
					RtlZeroMemory(pSettingList, sizeof(DETECT_SETTING_LIST));
					// �v���Z�X�����i�[����B
					pSettingList->ProcessName = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, (strlen(szTemp)+ 1) * sizeof(WCHAR), 'TSET');
					if( pSettingList->ProcessName != NULL){
						RtlZeroMemory(pSettingList->ProcessName, (strlen(szTemp)+ 1) * sizeof(WCHAR));
						RtlMultiByteToUnicodeN( wcsTemp, _MAX_PATH*sizeof(WCHAR), NULL, szTemp, strlen(szTemp)+1);
						wcsTemp[strlen(szTemp)] =0;
						UTIL_UnicodeStringUpper(wcsTemp);
						RtlStringCchCopyW(pSettingList->ProcessName, (strlen(szTemp)+ 1) * sizeof(WCHAR), wcsTemp);	
					}
					break;
					
				case 2:
					// �v���Z�X�������i�[����
					pSettingList->ProcessAttributes = atoi(szTemp);
					break;
					
				case 3:
				case 4:
					// �t�@�C�������i�[����
					if(strlen(szTemp) != 0){
						pSettingList->FullPath.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, (strlen(szTemp)+ 1) * sizeof(WCHAR),'TSET');
						if( pSettingList->FullPath.Buffer != NULL){
							RtlZeroMemory( pSettingList->FullPath.Buffer,  (strlen(szTemp)+ 1) * sizeof(WCHAR));
							RtlMultiByteToUnicodeN( wcsTemp, _MAX_PATH*sizeof(WCHAR), NULL, szTemp, strlen(szTemp)+1);
							wcsTemp[strlen(szTemp)] =0;
							UTIL_UnicodeStringUpper(wcsTemp);
							pSettingList->FullPath.Length = 0;
							pSettingList->FullPath.MaximumLength = (strlen(szTemp)+ 1) * sizeof(WCHAR);
							RtlAppendUnicodeToString( &pSettingList->FullPath, wcsTemp);
							//RtlCopyUnicodeString( &pSettingList->FullPath, wcsTemp);
						}
					}
					if(nSearchCount == 4){
						szSearch[0] = ';';
					}
					break;
					
				case 5:
					// �A�N�Z�X�������i�[����
					pSettingList->AccessAttributes = atoi(szTemp);
					
					// ���X�g�ɏ���ǉ�����
					KeAcquireSpinLock(&gDetectListLick, &oldIrql);
					InsertHeadList(&gDetectList.ListEntry, &pSettingList->ListEntry);
		            KeReleaseSpinLock( &gDetectListLick, oldIrql );
		            
		            // �I�t�Z�b�g��ݒ肷��
		            *pulReadOffset = pSearchPtr - (PCHAR)pReadBuffer;
		            // �����J�E���g��0�ɂ���
					nSearchCount = 0;
					// �����������u,�v�ɂ���
					szSearch[0] = ',';
					// ���̃��X�g�����������m�ۂ���
					pSettingList = ExAllocatePoolWithTag(NonPagedPool, sizeof(DETECT_SETTING_LIST), 'TSIL');
					break;
				default:
					break;
			}
			nSearchCount++;
			pSearchPtr++;
			// �擪�̃X�y�[�X�A�^�u�A���s�͖�������
			while( *pSearchPtr == ' ' || *pSearchPtr == '\t' || *pSearchPtr == '\n' || *pSearchPtr == '\r' ){
				pSearchPtr++;
			}
			pStartPtr = pSearchPtr;
			pSearchPtr = strchr((char*)pSearchPtr, szSearch[0]);				
		}
		
		// �Ō�̕����񂪑��݂��邩�ǂ������m�F����
		szSearch[0] = ';';
		if( pSearchPtr == NULL){
			// �Ō�̕�����̏ꍇ�A�O��̃|�C���^���g�p����
			pSearchPtr = pStartPtr;
		}
		pSearchPtr = strchr((char*)pSearchPtr, szSearch[0]);
		if(pSearchPtr != NULL){
			// �Ō�̕����񔭌�
			*pulReadOffset = 0;
		}
		else {
			// �Ō�̕����񖳂����߂��̂܂ܐi�ށB���X�g�p�̃������͉������B
			if(pSettingList != NULL){
				ExFreePool(pSettingList);
			}
		}
	}
	else{
		status = STATUS_INVALID_PARAMETER;
	}
	
	return status;
}

NTSTATUS GetAccessCondition(
			PFILE_ACCESS_DATA	pFileAccessData)
{
	NTSTATUS					status;
	PDETECT_SETTING_LIST	pSettingList;
	
	
	// �����͗L���H
	if( pFileAccessData != NULL){
		status = STATUS_SUCCESS;
		pSettingList = (PDETECT_SETTING_LIST)gDetectList.ListEntry.Flink;
		
		// ���̃��X�g�����݂���ԁA���[�v����
		while((pSettingList != NULL) && (pSettingList != &gDetectList)){
		
			// �v���Z�X������v����H(���S��v)
			if(wcslen(pSettingList->ProcessName) == wcslen(pFileAccessData->ProcessName)){
				if(RtlCompareMemory( pSettingList->ProcessName, pFileAccessData->ProcessName, wcslen(pFileAccessData->ProcessName) * sizeof(WCHAR) + sizeof(WCHAR))
						==  wcslen(pFileAccessData->ProcessName) * sizeof(WCHAR) + sizeof(WCHAR)){
			
					// �v���Z�X����v
					// �t�@�C��������v����H(�O����v)
					if( RtlCompareMemory( pSettingList->FullPath.Buffer, pFileAccessData->FullPath.Buffer, pSettingList->FullPath.Length)
						==  pSettingList->FullPath.Length){
			
						// �t�@�C��������v����B
						// ���X�g�̏���Ώۃt�@�C���A�N�Z�X�f�[�^�Ɋi�[����
						pFileAccessData->AccessAttributes = pSettingList->AccessAttributes;
						pFileAccessData->ProcessAttributes = pSettingList->ProcessAttributes;
						break;
					}
					else{
						// �t�@�C��������v���Ȃ�
					}
				}
				else{
					// �v���Z�X������v���Ȃ�
				}
			}
			else {
				// �v���Z�X��������v���Ȃ�
			}
			pSettingList = (PDETECT_SETTING_LIST)pSettingList->ListEntry.Flink;
		}
	}
	else{
		// ����������
		status = STATUS_INVALID_PARAMETER;
	}

	return status;
}

BOOLEAN IsZeroAttack(
			PFILE_ACCESS_DATA	pFileAccessData,
			ULONG				ulAccessType)
{
	BOOLEAN		bIsZeroAttack = FALSE;
	PDETECT_SETTING_LIST	pSettingList;
	
	
	// �����͗L���H
	if( pFileAccessData != NULL){
		//�@�t�@�C���̃A�N�Z�X����
		switch( pFileAccessData->AccessAttributes){
		case DISABLE_WRITE:
			if(ulAccessType == WRITE_OPERATION){
				// �������ݗv��
				bIsZeroAttack = TRUE;
			}
			break;
		case DISABLE_READ:
			bIsZeroAttack = TRUE;
			break;
		default:
			break;
		}
	}
	else {
		// ���������B�������Ȃ�
	}

	return bIsZeroAttack;
}

NTSTATUS	NotifyAttackData(
			PFLT_CALLBACK_DATA	Data,
			PFILE_ACCESS_DATA	pFileAccessData,
			ULONG				ulAccessType)
{
	NTSTATUS		status;
	PCHAR			pWriteData;
	ULONG			ulAllocSize;
	UNICODE_STRING	FileName;
	
	// �����͗L���H
	if( Data != NULL && pFileAccessData != NULL){
		// �������ރf�[�^��ێ����郁�������m��
		ulAllocSize = sizeof(FILE_ACCESS_DATA) + TIME_SIZE + wcslen(pFileAccessData->ProcessName) + pFileAccessData->FullPath.MaximumLength;
		pWriteData = ExAllocatePoolWithTag( NonPagedPool, ulAllocSize , 'ETIR');
		if(pWriteData != NULL){
			// �������ރf�[�^�̍\�z
			RtlZeroMemory(pWriteData, ulAllocSize);
			status = ConstructWriteAttackData( pWriteData, pFileAccessData, ulAccessType);
			RtlInitUnicodeString( &FileName, L"\\DosDevices\\C:\\test\\DetectZAttack.txt");
			// �f�[�^�̏�������
			RtlStringCbLengthA( pWriteData, ulAllocSize, &ulAllocSize);
			status = WriteFileData( FileName, &ulAllocSize, pWriteData);
			ExFreePool(pWriteData);
		}
		else{
			status = STATUS_INSUFFICIENT_RESOURCES;
		}

//		DbgPrint("Detected Zeroday Attack:%wZ FileName=%wZ, ProcessName=%ws, FileAttributes=%8.8lx, FileAccess=%8.8lx\n", 
//				pFileAccessData->FullPath, pFileAccessData->ProcessName, pFileAccessData->ProcessAttributes, ulAccessType);
		
	}
	else {
		// �����͖���
		status = STATUS_INVALID_PARAMETER;
	}
	
	return status;
}

NTSTATUS	ConstructWriteAttackData(
			PCHAR				pWriteData,
			PFILE_ACCESS_DATA	pFileAccessData,
			ULONG				ulAccessType)
{
	NTSTATUS	status = STATUS_UNSUCCESSFUL;
	ULONG		ulAllocSize;
	PCHAR		pTemp;
	
	KdPrint(("DetectZ : NotifyAttackData Enter\n"));
	
	// �����͗L���H
	if( pWriteData != NULL && pFileAccessData != NULL){
		// Passive���x���H
		if(KeGetCurrentIrql() == PASSIVE_LEVEL){
			// �ꎞ�������m��
			ulAllocSize = max( wcslen(pFileAccessData->ProcessName) * sizeof(WCHAR), pFileAccessData->FullPath.Length);
			pTemp = (PCHAR)ExAllocatePoolWithTag(NonPagedPool, ulAllocSize, 'PMET');
			if(pTemp != NULL){
				ulAllocSize = sizeof(FILE_ACCESS_DATA) + TIME_SIZE + wcslen(pFileAccessData->ProcessName) + pFileAccessData->FullPath.MaximumLength;
				// �������ރf�[�^����
				// �����擾
				status = GetCurrentTime(pWriteData);
				RtlStringCchCatA( pWriteData, ulAllocSize, ",");
				// �A�N�Z�X���e
				sprintf( pTemp, "%d,", ulAccessType);
				RtlStringCchCatA( pWriteData, ulAllocSize, pTemp);
				// �v���Z�X��
				wcstombs(pTemp, pFileAccessData->ProcessName, wcslen(pFileAccessData->ProcessName) * sizeof(WCHAR));
				RtlStringCchCatA( pWriteData, ulAllocSize, pTemp);
				RtlStringCchCatA( pWriteData, ulAllocSize, ",");
				// �v���Z�X�̃A�N�Z�X����
				sprintf( pTemp, "%d,", pFileAccessData->ProcessAttributes);
				RtlStringCchCatA( pWriteData, ulAllocSize, pTemp);
				// �t�@�C����
				wcstombs(pTemp, pFileAccessData->FullPath.Buffer, pFileAccessData->FullPath.Length);
				RtlStringCchCatA( pWriteData, ulAllocSize, pTemp);
				RtlStringCchCatA( pWriteData, ulAllocSize, ",");
				// �t�@�C���̑���
				sprintf( pTemp, "%d\r\n", pFileAccessData->AccessAttributes);
				RtlStringCchCatA( pWriteData, ulAllocSize, pTemp);
				
				ExFreePool(pTemp);
			}
			else {
				// �ꎞ�������m�ێ��s
				status = STATUS_INSUFFICIENT_RESOURCES;
			}
		}
	}
	else {
		// �����͖���
		status = STATUS_INVALID_PARAMETER;
	}
	
	KdPrint(("DetectZ : NotifyAttackData Exit\n"));
	
	return status;
}

NTSTATUS GetCurrentTime(
		PCHAR	pTime)
{
	NTSTATUS		status = STATUS_SUCCESS;
	LARGE_INTEGER	Time;
    TIME_FIELDS 	TimeFields;

	KdPrint(("DetectZ : NotifyAttackData Exit\n"));
	// �����͗L���H
	if(pTime != NULL){
		// ���݂̎����擾
		KeQuerySystemTime(&Time);
		ExSystemTimeToLocalTime(&Time, &Time);
 	    
 	    // TimeField�ɕϊ�
 	    (VOID)RtlTimeToTimeFields( &Time, &TimeFields );

		sprintf(pTime,"%4.4d/%2.2d/%2.2d %2.2d:%2.2d:%2.2d.%3.3d", 
			TimeFields.Year, TimeFields.Month, TimeFields.Day, TimeFields.Hour, TimeFields.Minute, TimeFields.Second, TimeFields.Milliseconds);
	}
	else {
		// �����͖���
		status = STATUS_INVALID_PARAMETER;
	}

	KdPrint(("DetectZ : NotifyAttackData Exit\n"));
	
	return status;
}

NTSTATUS	WriteFileData(
	UNICODE_STRING		FileName,
	PULONG				pulWriteSize,
	PVOID				pWriteBuffer)
{
	NTSTATUS			status;
	HANDLE				hFile;
	OBJECT_ATTRIBUTES	ObjectAttributes;
	IO_STATUS_BLOCK		IoStatusBlock;
	LARGE_INTEGER 		offset;	
	
	KdPrint(("DetectZ : WriteFileData Enter\n"));
	status = STATUS_SUCCESS;

	// �����͗L���H
	if( FileName.Buffer != NULL && pulWriteSize != NULL && pWriteBuffer != NULL){
		// Passive���x���H
		if(KeGetCurrentIrql() == PASSIVE_LEVEL){
			status = STATUS_SUCCESS;
		}
		else{
			// �s����IRQL
			status = STATUS_UNSUCCESSFUL;
		}
	}
	else {
		// ����������
		status = STATUS_INVALID_PARAMETER;
	}
	
	if( NT_SUCCESS(status)){
		
		InitializeObjectAttributes(
					&ObjectAttributes,
					&FileName,
					OBJ_CASE_INSENSITIVE,
					NULL,
					NULL);
		
		status = FltCreateFile(
					gFilterHandle,		// Filter
					NULL,				// Instance OPTIONAL
					&hFile,				// FileHandle
					FILE_APPEND_DATA,	// DesiredAccess
					&ObjectAttributes,	// ObjectAttributes
					&IoStatusBlock,		// IoStatusBlock
					NULL,				// AllocationSize OPTIONAL
					0L,					// FileAttributes
					FILE_SHARE_WRITE,	// ShareAccess
					FILE_OPEN_IF,		// CreateDisposition
					//FILE_NON_DIRECTORY_FILE, // CreateOptions
					FILE_SYNCHRONOUS_IO_NONALERT, // CreateOptions
					NULL,				// EaBuffer OPTIONAL
					0L,					// EaLength
					0					// Flags
					);
		if(NT_SUCCESS(status)){
			
			offset.HighPart = -1;
			offset.LowPart = FILE_WRITE_TO_END_OF_FILE;

			// �t�@�C������������
			status = ZwWriteFile( 
						hFile,
						NULL,
						NULL,
						NULL,
						&IoStatusBlock,
						pWriteBuffer,
						*pulWriteSize,
						&offset,
						NULL);
			FltClose(hFile);
		}
	}
	   	
	KdPrint(("DetectZ : WriteFileData Exit\n"));

	return status;
}
