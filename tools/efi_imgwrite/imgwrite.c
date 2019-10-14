

#include <efi.h>
#include <efilib.h>
#include <efishellparm.h>


#define PARAMETER_MAX_CHAR_COUNT (64)
#define BUFFER_SIZE              (4096 * 4)

static const EFI_GUID simpleFileSystemGUID = SIMPLE_FILE_SYSTEM_PROTOCOL;
static const EFI_GUID loadedImageGUID = LOADED_IMAGE_PROTOCOL;
static const EFI_GUID blkGUID = EFI_BLOCK_IO_PROTOCOL_GUID;


int processParams(EFI_HANDLE ImageHandle, CHAR16* srcImgFileName);
EFI_FILE* openImageFile(EFI_HANDLE ImageHandle, CHAR16* srcImgFileName);
EFI_BLOCK_IO* inputTargetDevice();
UINTN writeImage(EFI_FILE* imgFile, EFI_BLOCK_IO* targetDevice);


EFI_STATUS EFIAPI efi_main(
  EFI_HANDLE ImageHandle,
  EFI_SYSTEM_TABLE *SystemTable
)
{
  CHAR16 srcImgFileName[PARAMETER_MAX_CHAR_COUNT];
  EFI_FILE* imgFile;
  EFI_BLOCK_IO* targetDevice;


  InitializeLib(ImageHandle, SystemTable);

  if (processParams(ImageHandle, srcImgFileName)) {
    Print(L"Source image file name: %s\n", srcImgFileName);
    imgFile = openImageFile(ImageHandle, srcImgFileName);
    if (imgFile) {
      Print(L"Open image file: ok\n");
      targetDevice = inputTargetDevice();
      if (targetDevice) {
        Print(L"Open target device: ok\n");
        writeImage(imgFile, targetDevice);
      }
    }
    else {
      Print(L"Failure to open image file.\n");
    }
  }
  Print(L"\n");
  return EFI_SUCCESS;
}

int processParams(
  EFI_HANDLE ImageHandle,
  CHAR16* srcImgFileName
) { 
  INTN argc;
  CHAR16** argv;

  argc = GetShellArgcArgv(ImageHandle, &argv);
  if (argc != 2) {
    Print(L"Usage: %s <image file name>\n", argv[0]);
    Print(
      L"No image file name is input. 'ROM.BIN' is applied instead.",
      argv[0]
    );
    StrCpy(srcImgFileName, L"ROM.BIN");
    return 1;
  }
  if (StrLen(argv[1]) >= PARAMETER_MAX_CHAR_COUNT) {
    return 0;
  }
  StrCpy(srcImgFileName, argv[1]);
  return 1;
}

EFI_FILE* openImageFile(EFI_HANDLE ImageHandle, CHAR16* srcImgFileName)
{
  EFI_STATUS status = EFI_SUCCESS;
  EFI_HANDLE deviceHandle = 0;
  EFI_LOADED_IMAGE* loadedImg = NULL;
  EFI_FILE_IO_INTERFACE* fileIO = NULL;
  EFI_FILE* root = NULL;
  EFI_FILE* imgFile = NULL;


  if (!srcImgFileName) {
    return NULL;
  }

  // Retrieve deviceHandle
  status = uefi_call_wrapper(
    BS->HandleProtocol,
    3,
    ImageHandle,
    &loadedImageGUID,
    &loadedImg
  );
  if (status != EFI_SUCCESS) {
    return NULL;
  }
  deviceHandle = loadedImg->DeviceHandle;

  // Retrieve file io interface
  status = uefi_call_wrapper(
    BS->HandleProtocol,
    3,
    deviceHandle,
    &simpleFileSystemGUID,
    &fileIO
  );
  if (status != EFI_SUCCESS) {
    return NULL;
  }

  // Retrieve root folder
  status = uefi_call_wrapper(
    fileIO->OpenVolume,
    2,
    fileIO,
    &root
  );
  if (status != EFI_SUCCESS) {
    return NULL;
  }

  // Open image file
  status = uefi_call_wrapper(
    root->Open,
    5,
    root,
    &imgFile,
    srcImgFileName,
    EFI_FILE_MODE_READ,
  0);
  if (status != EFI_SUCCESS) {
    return NULL;
  }

  return imgFile;
}

EFI_BLOCK_IO* inputTargetDevice()
{
  UINTN i = 0;
  EFI_STATUS status;
  UINTN handleCount = 0;
  EFI_HANDLE handle = 0;
  EFI_HANDLE* handles = NULL;
  EFI_BLOCK_IO* rawBlkIo = NULL;
  EFI_DEVICE_PATH *devPath = NULL;
  CHAR16 buffer[4];


  // Retrieve all raw block io devices
  status = uefi_call_wrapper(
    BS->LocateHandleBuffer,
    5,
    ByProtocol,
    &blkGUID,
    NULL,
    &handleCount,
    &handles
  );

  if (status != EFI_SUCCESS) {
    goto Failure;
  }

  Print(L"List of block devices:\n", handleCount);
  for (i = 0; i < handleCount; i++) {
    handle = handles[i];
    devPath = DevicePathFromHandle(handle);
    Print(L"  %d. %s\n", (i + 1), DevicePathToStr(devPath));
  }
  
  Print(L"Input target device (1..%d) or 0 to exit: ", handleCount);
  Input(NULL, buffer, 4);
  Print(L"\n");
  i = xtoi(buffer);
  if (i == 0 || i > handleCount) {
    goto Failure;
  }

  handle = handles[i - 1];
  status = uefi_call_wrapper(
    BS->HandleProtocol,
    3,
    handle,
    &blkGUID,
    &rawBlkIo
  );

  FreePool(handles);
  return rawBlkIo;
  
Failure:
  if (handles) {
    FreePool(handles);
  }
  return NULL;
}

UINTN writeImage(EFI_FILE* imgFile, EFI_BLOCK_IO* targetDevice)
{
  EFI_STATUS status;
  uint8_t buffer[BUFFER_SIZE];
  UINTN i = 0;
  UINTN bufferSize;
  UINTN blockSize = targetDevice->Media->BlockSize;
  EFI_FILE_INFO* imgFileInfo = LibFileInfo(imgFile);
  UINT64 fileSize = imgFileInfo->FileSize;
  UINT64 step = fileSize / 50;//100;
  UINT64 boundary = step;
  UINT64 curPosition = 0;


  if (blockSize > BUFFER_SIZE) {
    return 0;
  }

  Print(L"Writing %d MiB...\n", (int)(fileSize / (1024 * 1024)));
  Print(L"[-------------------------------------------------]");
  status = uefi_call_wrapper(
    ST->ConOut->SetCursorPosition,
    3,
    ST->ConOut,
    1,
    ST->ConOut->Mode->CursorRow
  );
  if (status != EFI_SUCCESS) {
    return 0;
  }

  while(1){
    bufferSize = blockSize;
    status = uefi_call_wrapper(
      imgFile->Read,
      3,
      imgFile,
      &bufferSize,
      buffer
    );
    if (!bufferSize) {
      break;
    }
    if (status != EFI_SUCCESS) {
      break;
    }
    status = uefi_call_wrapper(
      targetDevice->WriteBlocks,
      3,
      targetDevice,
      targetDevice->Media->MediaId,
      i,
      blockSize,
      buffer
    );
    curPosition += blockSize;
    if (curPosition >= boundary) {
      boundary += step;
      Print(L"#");
    }
    i += 1;
    if (curPosition > fileSize) {
      break;
    }
  }
  Print(L"]\n");
  uefi_call_wrapper(imgFile->Close, 1, imgFile);
  uefi_call_wrapper(targetDevice->FlushBlocks, 1, targetDevice);
  return 1;
}


