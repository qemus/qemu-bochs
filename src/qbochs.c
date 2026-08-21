/*
 * PROJECT:     QBochs
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     QEMU/Bochs DISPI display miniport for Windows NT 5.x
 * COPYRIGHT:   Copyright 2022 Hervé Poussineau <hpoussin@reactos.org>
 *
 * Based on the ReactOS Bochs graphics card driver.
 */

#include "qbochs.h"

#define QBOCHS_WRITE_COMBINE_READY L"QBochsWriteCombineReady"
#define QBOCHS_BOOT_UNCACHED_KEY L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\GraphicsDrivers\\QBochsUncached"
#define QBOCHS_BOOT_WRITE_COMBINED_KEY L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\GraphicsDrivers\\QBochsWriteCombined"

static const QBOCHS_SIZE QBochsAvailableResolutions[] = {
    { 640, 480, 32 },   /* VGA */
    { 640, 480, 16 },
    { 800, 600, 32 },   /* SVGA */
    { 800, 600, 16 },
    { 1024, 600, 32 },  /* WSVGA */
    { 1024, 600, 16 },
    { 1024, 768, 32 },  /* XGA */
    { 1024, 768, 16 },
    { 1152, 864, 32 },  /* XGA+ */
    { 1152, 864, 16 },
    { 1280, 720, 32 },  /* WXGA-H */
    { 1280, 720, 16 },
    { 1280, 768, 32 },  /* WXGA */
    { 1280, 768, 16 },
    { 1280, 960, 32 },  /* SXGA- */
    { 1280, 960, 16 },
    { 1280, 1024, 32 }, /* SXGA */
    { 1280, 1024, 16 },
    { 1368, 768, 32 },  /* HD ready */
    { 1368, 768, 16 },
    { 1400, 1050, 32 }, /* SXGA+ */
    { 1400, 1050, 16 },
    { 1440, 900, 32 },  /* WSXGA */
    { 1440, 900, 16 },
    { 1600, 900, 32 },  /* HD+ */
    { 1600, 900, 16 },
    { 1600, 1200, 32 }, /* UXGA */
    { 1600, 1200, 16 },
    { 1680, 1050, 32 }, /* WSXGA+ */
    { 1680, 1050, 16 },
    { 1920, 1080, 32 }, /* FHD */
    { 1920, 1080, 16 },
    { 2048, 1536, 32 }, /* QXGA */
    { 2048, 1536, 16 },
    { 2560, 1440, 32 }, /* WQHD */
    { 2560, 1440, 16 },
    { 2560, 1600, 32 }, /* WQXGA */
    { 2560, 1600, 16 },
    { 2560, 2048, 32 }, /* QSXGA */
    { 2560, 2048, 16 },
    { 2800, 2100, 32 }, /* QSXGA+ */
    { 2800, 2100, 16 },
    { 3200, 2400, 32 }, /* QUXGA */
    { 3200, 2400, 16 },
    { 3840, 2160, 32 }, /* 4K UHD-1 */
    { 3840, 2160, 16 },
};

CODE_SEG("PAGE")
static VOID
QBochsWriteDispI(
    _In_ PQBOCHS_DEVICE_EXTENSION DeviceExtension,
    _In_ ULONG Index,
    _In_ USHORT Value)
{
    if (DeviceExtension->IoPorts.RangeInIoSpace)
    {
        VideoPortWritePortUshort((PUSHORT)(DeviceExtension->IoPorts.Mapped - VBE_DISPI_IOPORT_INDEX + VBE_DISPI_IOPORT_INDEX), Index);
        VideoPortWritePortUshort((PUSHORT)(DeviceExtension->IoPorts.Mapped - VBE_DISPI_IOPORT_INDEX + VBE_DISPI_IOPORT_DATA), Value);
    }
    else
    {
        VideoPortWriteRegisterUshort((PUSHORT)(DeviceExtension->IoPorts.Mapped + 0x500 + Index * 2), Value);
    }
}

CODE_SEG("PAGE")
static USHORT
QBochsReadDispI(
    _In_ PQBOCHS_DEVICE_EXTENSION DeviceExtension,
    _In_ ULONG Index)
{
    if (DeviceExtension->IoPorts.RangeInIoSpace)
    {
        VideoPortWritePortUshort((PUSHORT)(DeviceExtension->IoPorts.Mapped - VBE_DISPI_IOPORT_INDEX + VBE_DISPI_IOPORT_INDEX), Index);
        return VideoPortReadPortUshort((PUSHORT)(DeviceExtension->IoPorts.Mapped - VBE_DISPI_IOPORT_INDEX + VBE_DISPI_IOPORT_DATA));
    }

    return VideoPortReadRegisterUshort((PUSHORT)(DeviceExtension->IoPorts.Mapped + 0x500 + Index * 2));
}

CODE_SEG("PAGE")
static BOOLEAN
QBochsWriteDispIAndCheck(
    _In_ PQBOCHS_DEVICE_EXTENSION DeviceExtension,
    _In_ ULONG Index,
    _In_ USHORT Value)
{
    QBochsWriteDispI(DeviceExtension, Index, Value);
    return QBochsReadDispI(DeviceExtension, Index) == Value;
}


CODE_SEG("PAGE")
static VP_STATUS NTAPI
QBochsReadWriteCombineReady(
    _In_ PVOID HwDeviceExtension,
    _In_ PVOID Context,
    _In_ PWSTR ValueName,
    _In_ PVOID ValueData,
    _In_ ULONG ValueLength)
{
    PBOOLEAN Ready = Context;

    (VOID)HwDeviceExtension;
    (VOID)ValueName;

    if (ValueLength >= sizeof(ULONG))
        *Ready = (*(PULONG)ValueData != 0);

    return NO_ERROR;
}

CODE_SEG("PAGE")
static BOOLEAN
QBochsBootKeyExists(
    _In_ PCWSTR KeyName)
{
    UNICODE_STRING KeyPath;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE KeyHandle;
    NTSTATUS Status;

    RtlInitUnicodeString(&KeyPath, KeyName);
    InitializeObjectAttributes(&ObjectAttributes,
                               &KeyPath,
                               OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);

    Status = ZwOpenKey(&KeyHandle, KEY_READ, &ObjectAttributes);
    if (!NT_SUCCESS(Status))
        return FALSE;

    ZwClose(KeyHandle);
    return TRUE;
}

CODE_SEG("PAGE")
static BOOLEAN
QBochsCreateBootKey(
    _In_ PCWSTR KeyName)
{
    UNICODE_STRING KeyPath;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE KeyHandle;
    NTSTATUS Status;

    RtlInitUnicodeString(&KeyPath, KeyName);
    InitializeObjectAttributes(&ObjectAttributes,
                               &KeyPath,
                               OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);

    Status = ZwCreateKey(&KeyHandle,
                         READ_CONTROL,
                         &ObjectAttributes,
                         0,
                         NULL,
                         REG_OPTION_VOLATILE,
                         NULL);
    if (!NT_SUCCESS(Status))
    {
        VideoDebugPrint((Error, "QBochs: failed to create volatile boot cache-policy key (0x%08lx)\n", Status));
        return FALSE;
    }

    ZwClose(KeyHandle);
    return TRUE;
}

CODE_SEG("PAGE")
static BOOLEAN
QBochsReadBootCachePolicy(
    _Inout_ PQBOCHS_DEVICE_EXTENSION DeviceExtension)
{
    BOOLEAN Uncached;
    BOOLEAN WriteCombined;

    Uncached = QBochsBootKeyExists(QBOCHS_BOOT_UNCACHED_KEY);
    WriteCombined = QBochsBootKeyExists(QBOCHS_BOOT_WRITE_COMBINED_KEY);

    if (!Uncached && !WriteCombined)
        return FALSE;

    if (Uncached)
    {
        DeviceExtension->BootCachePolicy = QBochsBootCachePolicyUncached;
        if (WriteCombined)
            VideoDebugPrint((Error, "QBochs: conflicting volatile boot cache-policy keys; forcing uncached\n"));
    }
    else
    {
        DeviceExtension->BootCachePolicy = QBochsBootCachePolicyWriteCombined;
    }

    DeviceExtension->BootPolicyLatched = TRUE;
    return TRUE;
}

CODE_SEG("PAGE")
static BOOLEAN
QBochsLatchBootCachePolicy(
    _Inout_ PQBOCHS_DEVICE_EXTENSION DeviceExtension)
{
    PCWSTR KeyName;

    KeyName = DeviceExtension->BootCachePolicy == QBochsBootCachePolicyWriteCombined
        ? QBOCHS_BOOT_WRITE_COMBINED_KEY
        : QBOCHS_BOOT_UNCACHED_KEY;

    if (!QBochsCreateBootKey(KeyName))
        return FALSE;

    DeviceExtension->BootPolicyLatched = TRUE;
    return TRUE;
}

CODE_SEG("PAGE")
static BOOLEAN
QBochsGetControllerInfo(
    _Inout_ PQBOCHS_DEVICE_EXTENSION DeviceExtension)
{
    USHORT Version;
    WCHAR ChipType[5];
    ULONG SizeInBytes;

    for (Version = VBE_DISPI_ID5; Version >= VBE_DISPI_ID0; Version--)
    {
        if (QBochsWriteDispIAndCheck(DeviceExtension, VBE_DISPI_INDEX_ID, Version))
            break;
    }

    if (Version < VBE_DISPI_ID0)
    {
        VideoDebugPrint((Error, "QBochs: VBE extension signature incorrect\n"));
        return FALSE;
    }

    if (Version < VBE_DISPI_ID2)
    {
        VideoDebugPrint((Error, "QBochs: VBE extension too old (0x%04x)\n", Version));
        return FALSE;
    }

    if (Version <= VBE_DISPI_ID2)
    {
        DeviceExtension->MaxXResolution = 1024;
        DeviceExtension->MaxYResolution = 768;
    }
    else
    {
        QBochsWriteDispI(DeviceExtension, VBE_DISPI_INDEX_ENABLE, VBE_DISPI_GETCAPS);
        DeviceExtension->MaxXResolution = QBochsReadDispI(DeviceExtension, VBE_DISPI_INDEX_XRES);
        DeviceExtension->MaxYResolution = QBochsReadDispI(DeviceExtension, VBE_DISPI_INDEX_YRES);
        QBochsWriteDispI(DeviceExtension, VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);

        /* QEMU bochs-display can report zero for GETCAPS. */
        if (DeviceExtension->MaxXResolution == 0 && DeviceExtension->MaxYResolution == 0)
        {
            DeviceExtension->MaxXResolution = 1024;
            DeviceExtension->MaxYResolution = 768;
        }
    }

    if (Version < VBE_DISPI_ID4)
    {
        DeviceExtension->VramSize64K = 4 * 1024 / 64;
    }
    else if (Version == VBE_DISPI_ID4)
    {
        DeviceExtension->VramSize64K = 8 * 1024 / 64;
    }
    else
    {
        DeviceExtension->VramSize64K = QBochsReadDispI(DeviceExtension, VBE_DISPI_INDEX_VIDEO_MEMORY_64K);
    }

    if (DeviceExtension->VramSize64K == 0)
    {
        DeviceExtension->VramSize64K = DeviceExtension->FrameBuffer.RangeLength / (64 * 1024);
    }

#define HEX(c) (((c) >= 0 && (c) <= 9) ? (c) + L'0' : (c) - 10 + L'A')
    ChipType[0] = HEX((Version >> 12) & 0xf);
    ChipType[1] = HEX((Version >> 8) & 0xf);
    ChipType[2] = HEX((Version >> 4) & 0xf);
    ChipType[3] = HEX(Version & 0xf);
    ChipType[4] = UNICODE_NULL;
#undef HEX

    VideoPortSetRegistryParameters(DeviceExtension, L"HardwareInformation.ChipType", ChipType, sizeof(ChipType));
    SizeInBytes = DeviceExtension->VramSize64K * 64 * 1024;
    VideoPortSetRegistryParameters(DeviceExtension, L"HardwareInformation.MemorySize", &SizeInBytes, sizeof(SizeInBytes));

    VideoDebugPrint((Info, "QBochs: DISPI 0x%04x, %dx%d max, %lu MB VRAM\n",
                     Version,
                     DeviceExtension->MaxXResolution,
                     DeviceExtension->MaxYResolution,
                     SizeInBytes / (1024 * 1024)));
    return TRUE;
}

CODE_SEG("PAGE")
static BOOLEAN
QBochsInitializeModes(
    _Inout_ PQBOCHS_DEVICE_EXTENSION DeviceExtension)
{
    ULONG Index;
    ULONG ModeCount = 0;
    ULONG VramBytes = DeviceExtension->VramSize64K * 64 * 1024;

    VideoPortZeroMemory(DeviceExtension->AvailableModeInfo, sizeof(DeviceExtension->AvailableModeInfo));

    for (Index = 0; Index < ARRAYSIZE(QBochsAvailableResolutions); Index++)
    {
        ULONG RequiredBytes;

        if (QBochsAvailableResolutions[Index].XResolution > DeviceExtension->MaxXResolution)
            continue;
        if (QBochsAvailableResolutions[Index].YResolution > DeviceExtension->MaxYResolution)
            continue;

        RequiredBytes = QBochsAvailableResolutions[Index].XResolution *
                        QBochsAvailableResolutions[Index].YResolution *
                        (QBochsAvailableResolutions[Index].BitsPerPixel / 8);
        if (RequiredBytes > VramBytes)
            continue;

        if (ModeCount >= ARRAYSIZE(DeviceExtension->AvailableModeInfo))
            break;

        DeviceExtension->AvailableModeInfo[ModeCount++] = QBochsAvailableResolutions[Index];
    }

    DeviceExtension->AvailableModeCount = ModeCount;
    DeviceExtension->CurrentMode = 0;

    if (ModeCount == 0)
    {
        VideoDebugPrint((Error, "QBochs: no suitable video modes available\n"));
        return FALSE;
    }

    return TRUE;
}

CODE_SEG("PAGE")
static VOID
QBochsGetModeInfo(
    _In_ PQBOCHS_SIZE AvailableModeInfo,
    _Out_ PVIDEO_MODE_INFORMATION ModeInfo,
    _In_ ULONG Index)
{
    ModeInfo->Length = sizeof(*ModeInfo);
    ModeInfo->ModeIndex = Index;
    ModeInfo->VisScreenWidth = AvailableModeInfo->XResolution;
    ModeInfo->VisScreenHeight = AvailableModeInfo->YResolution;
    ModeInfo->ScreenStride = AvailableModeInfo->XResolution * (AvailableModeInfo->BitsPerPixel / 8);
    ModeInfo->NumberOfPlanes = 1;
    ModeInfo->BitsPerPlane = AvailableModeInfo->BitsPerPixel;
    ModeInfo->Frequency = 60;
    ModeInfo->XMillimeter = AvailableModeInfo->XResolution * 254 / 960;
    ModeInfo->YMillimeter = AvailableModeInfo->YResolution * 254 / 960;

    if (AvailableModeInfo->BitsPerPixel == 16)
    {
        ModeInfo->NumberRedBits = 5;
        ModeInfo->NumberGreenBits = 6;
        ModeInfo->NumberBlueBits = 5;
        ModeInfo->RedMask = 0xf800;
        ModeInfo->GreenMask = 0x07e0;
        ModeInfo->BlueMask = 0x001f;
    }
    else
    {
        ModeInfo->NumberRedBits = 8;
        ModeInfo->NumberGreenBits = 8;
        ModeInfo->NumberBlueBits = 8;
        ModeInfo->RedMask = 0xff0000;
        ModeInfo->GreenMask = 0x00ff00;
        ModeInfo->BlueMask = 0x0000ff;
    }

    ModeInfo->AttributeFlags = VIDEO_MODE_GRAPHICS | VIDEO_MODE_COLOR |
                               VIDEO_MODE_LINEAR | VIDEO_MODE_NO_OFF_SCREEN;
    ModeInfo->VideoMemoryBitmapWidth = AvailableModeInfo->XResolution;
    ModeInfo->VideoMemoryBitmapHeight = AvailableModeInfo->YResolution;
}

CODE_SEG("PAGE")
static BOOLEAN
QBochsMapVideoMemory(
    _In_ PQBOCHS_DEVICE_EXTENSION DeviceExtension,
    _In_ PVIDEO_MEMORY RequestedAddress,
    _Out_ PVIDEO_MEMORY_INFORMATION MapInformation,
    _Out_ PSTATUS_BLOCK StatusBlock)
{
    VP_STATUS Status;
    PHYSICAL_ADDRESS VideoMemory;
    ULONG MemSpace;

    ULONG VideoRamLength;

    VideoMemory = DeviceExtension->FrameBuffer.RangeStart;
    VideoRamLength =
        (DeviceExtension->AvailableModeInfo[DeviceExtension->CurrentMode].BitsPerPixel / 8) *
        DeviceExtension->AvailableModeInfo[DeviceExtension->CurrentMode].XResolution *
        DeviceExtension->AvailableModeInfo[DeviceExtension->CurrentMode].YResolution;
    MapInformation->VideoRamBase = RequestedAddress->RequestedVirtualAddress;
    MapInformation->VideoRamLength = VideoRamLength;

    /*
     * The volatile boot latch chooses one request policy for the entire boot.
     * This prevents a live VGA-to-QBochs switch from requesting P6CACHE while
     * still allowing WC on the first QBochs activation after a reboot.
     */
    MemSpace = VIDEO_MEMORY_SPACE_MEMORY;
    if (DeviceExtension->BootCachePolicy == QBochsBootCachePolicyWriteCombined)
        MemSpace |= VIDEO_MEMORY_SPACE_P6CACHE;

    Status = VideoPortMapMemory(DeviceExtension,
                                VideoMemory,
                                &MapInformation->VideoRamLength,
                                &MemSpace,
                                &MapInformation->VideoRamBase);

    if (Status != NO_ERROR)
    {
        StatusBlock->Status = Status;
        return FALSE;
    }

    if (!DeviceExtension->WriteCombiningReady && DeviceExtension->BootPolicyLatched)
    {
        ULONG Ready = 1;

        if (VideoPortSetRegistryParameters(DeviceExtension,
                                           QBOCHS_WRITE_COMBINE_READY,
                                           &Ready,
                                           sizeof(Ready)) == NO_ERROR)
        {
            DeviceExtension->WriteCombiningReady = TRUE;
        }
        else
        {
            VideoDebugPrint((Error, "QBochs: failed to arm write combining for the next boot\n"));
        }
    }

    MapInformation->FrameBufferBase = MapInformation->VideoRamBase;
    MapInformation->FrameBufferLength = MapInformation->VideoRamLength;
    StatusBlock->Information = sizeof(*MapInformation);
    StatusBlock->Status = NO_ERROR;
    return TRUE;
}

CODE_SEG("PAGE")
static BOOLEAN NTAPI
QBochsUnmapVideoMemory(
    _In_ PQBOCHS_DEVICE_EXTENSION DeviceExtension,
    _In_ PVIDEO_MEMORY VideoMemory,
    _Out_ PSTATUS_BLOCK StatusBlock)
{
    VP_STATUS Status;

    Status = VideoPortUnmapMemory(DeviceExtension, VideoMemory->RequestedVirtualAddress, NULL);
    StatusBlock->Status = Status;
    return (Status == NO_ERROR);
}

CODE_SEG("PAGE")
static BOOLEAN
QBochsQueryNumAvailableModes(
    _In_ PQBOCHS_DEVICE_EXTENSION DeviceExtension,
    _Out_ PVIDEO_NUM_MODES AvailableModes,
    _Out_ PSTATUS_BLOCK StatusBlock)
{
    AvailableModes->NumModes = DeviceExtension->AvailableModeCount;
    AvailableModes->ModeInformationLength = sizeof(VIDEO_MODE_INFORMATION);
    StatusBlock->Information = sizeof(*AvailableModes);
    StatusBlock->Status = NO_ERROR;
    return TRUE;
}

CODE_SEG("PAGE")
static BOOLEAN
QBochsQueryAvailableModes(
    _In_ PQBOCHS_DEVICE_EXTENSION DeviceExtension,
    _Out_ PVIDEO_MODE_INFORMATION ReturnedModes,
    _Out_ PSTATUS_BLOCK StatusBlock)
{
    ULONG Count;

    for (Count = 0; Count < DeviceExtension->AvailableModeCount; Count++)
    {
        VideoPortZeroMemory(&ReturnedModes[Count], sizeof(ReturnedModes[Count]));
        QBochsGetModeInfo(&DeviceExtension->AvailableModeInfo[Count], &ReturnedModes[Count], Count);
    }

    StatusBlock->Information = sizeof(VIDEO_MODE_INFORMATION) * DeviceExtension->AvailableModeCount;
    StatusBlock->Status = NO_ERROR;
    return TRUE;
}

CODE_SEG("PAGE")
static BOOLEAN
QBochsSetCurrentMode(
    _In_ PQBOCHS_DEVICE_EXTENSION DeviceExtension,
    _In_ PVIDEO_MODE RequestedMode,
    _Out_ PSTATUS_BLOCK StatusBlock)
{
    PQBOCHS_SIZE AvailableModeInfo;
    ULONG ModeRequested = RequestedMode->RequestedMode & 0x3fffffff;
    BOOLEAN Ret;

    if (ModeRequested >= DeviceExtension->AvailableModeCount)
    {
        StatusBlock->Status = ERROR_INVALID_PARAMETER;
        return FALSE;
    }

    AvailableModeInfo = &DeviceExtension->AvailableModeInfo[ModeRequested];

    QBochsWriteDispI(DeviceExtension, VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    Ret = QBochsWriteDispIAndCheck(DeviceExtension, VBE_DISPI_INDEX_XRES, AvailableModeInfo->XResolution) &&
          QBochsWriteDispIAndCheck(DeviceExtension, VBE_DISPI_INDEX_YRES, AvailableModeInfo->YResolution) &&
          QBochsWriteDispIAndCheck(DeviceExtension, VBE_DISPI_INDEX_BPP, AvailableModeInfo->BitsPerPixel);
    QBochsWriteDispI(DeviceExtension,
                     VBE_DISPI_INDEX_ENABLE,
                     VBE_DISPI_LFB_ENABLED | VBE_DISPI_ENABLED | VBE_DISPI_NOCLEARMEM);

    if (!Ret)
    {
        StatusBlock->Status = ERROR_INVALID_PARAMETER;
        return FALSE;
    }

    /* QEMU secondary-vga keeps VGA disabled until the guest explicitly enables it. */
    if (!DeviceExtension->IoPorts.RangeInIoSpace)
    {
        (VOID)VideoPortReadRegisterUshort((PUSHORT)(DeviceExtension->IoPorts.Mapped + 0x41A));
        VideoPortWriteRegisterUshort((PUSHORT)(DeviceExtension->IoPorts.Mapped + 0x400), 0x20);
    }

    DeviceExtension->CurrentMode = (USHORT)ModeRequested;
    StatusBlock->Status = NO_ERROR;
    return TRUE;
}

CODE_SEG("PAGE")
static BOOLEAN
QBochsQueryCurrentMode(
    _In_ PQBOCHS_DEVICE_EXTENSION DeviceExtension,
    _Out_ PVIDEO_MODE_INFORMATION VideoModeInfo,
    _Out_ PSTATUS_BLOCK StatusBlock)
{
    if (DeviceExtension->CurrentMode >= DeviceExtension->AvailableModeCount)
    {
        StatusBlock->Status = ERROR_INVALID_PARAMETER;
        return FALSE;
    }

    VideoPortZeroMemory(VideoModeInfo, sizeof(*VideoModeInfo));
    QBochsGetModeInfo(&DeviceExtension->AvailableModeInfo[DeviceExtension->CurrentMode],
                      VideoModeInfo,
                      DeviceExtension->CurrentMode);
    StatusBlock->Information = sizeof(*VideoModeInfo);
    StatusBlock->Status = NO_ERROR;
    return TRUE;
}

CODE_SEG("PAGE")
VP_STATUS NTAPI
QBochsFindAdapter(
    _In_ PVOID HwDeviceExtension,
    _In_ PVOID HwContext,
    _In_ PWSTR ArgumentString,
    _In_ PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    _In_ PUCHAR Again)
{
    PQBOCHS_DEVICE_EXTENSION DeviceExtension = HwDeviceExtension;
    VIDEO_ACCESS_RANGE AccessRanges[2] = {0};

    *Again = 0;

    /* Only fields present since NT4 are used from ConfigInfo. */
    if (ConfigInfo->Length < SIZE_OF_NT4_VIDEO_PORT_CONFIG_INFO)
        return ERROR_INVALID_PARAMETER;

    if (VideoPortGetAccessRanges(DeviceExtension,
                                 0,
                                 NULL,
                                 ARRAYSIZE(AccessRanges),
                                 AccessRanges,
                                 NULL,
                                 NULL,
                                 NULL) != NO_ERROR)
    {
        return ERROR_DEV_NOT_EXIST;
    }

    DeviceExtension->FrameBuffer.RangeStart = AccessRanges[0].RangeStart;
    DeviceExtension->FrameBuffer.RangeLength = AccessRanges[0].RangeLength;
    DeviceExtension->FrameBuffer.RangeInIoSpace = AccessRanges[0].RangeInIoSpace;

    if (AccessRanges[1].RangeLength == 0)
    {
        AccessRanges[1].RangeStart.LowPart = VBE_DISPI_IOPORT_INDEX;
        AccessRanges[1].RangeLength = 2;
        AccessRanges[1].RangeInIoSpace = TRUE;

        if (VideoPortVerifyAccessRanges(DeviceExtension, 1, &AccessRanges[1]) != NO_ERROR)
            return ERROR_DEV_NOT_EXIST;
    }
    else if (AccessRanges[1].RangeLength != 0x1000)
    {
        return ERROR_DEV_NOT_EXIST;
    }

    DeviceExtension->IoPorts.RangeStart = AccessRanges[1].RangeStart;
    DeviceExtension->IoPorts.RangeLength = AccessRanges[1].RangeLength;
    DeviceExtension->IoPorts.RangeInIoSpace = AccessRanges[1].RangeInIoSpace;

    DeviceExtension->IoPorts.Mapped = VideoPortGetDeviceBase(DeviceExtension,
                                                             DeviceExtension->IoPorts.RangeStart,
                                                             DeviceExtension->IoPorts.RangeLength,
                                                             DeviceExtension->IoPorts.RangeInIoSpace
                                                                 ? VIDEO_MEMORY_SPACE_IO
                                                                 : VIDEO_MEMORY_SPACE_MEMORY);
    if (!DeviceExtension->IoPorts.Mapped)
        return ERROR_DEV_NOT_EXIST;

    return NO_ERROR;
}

CODE_SEG("PAGE")
BOOLEAN NTAPI
QBochsInitialize(
    _In_ PVOID HwDeviceExtension)
{
    PQBOCHS_DEVICE_EXTENSION DeviceExtension = HwDeviceExtension;
    VP_STATUS Status;

    DeviceExtension->AvailableModeCount = 0;
    DeviceExtension->CurrentMode = 0;
    DeviceExtension->BootCachePolicy = QBochsBootCachePolicyUncached;
    DeviceExtension->BootPolicyLatched = FALSE;
    DeviceExtension->WriteCombiningReady = FALSE;

    Status = VideoPortGetRegistryParameters(DeviceExtension,
                                            QBOCHS_WRITE_COMBINE_READY,
                                            FALSE,
                                            QBochsReadWriteCombineReady,
                                            &DeviceExtension->WriteCombiningReady);

    if (QBochsReadBootCachePolicy(DeviceExtension))
    {
        VideoDebugPrint((Info,
                         DeviceExtension->BootCachePolicy == QBochsBootCachePolicyWriteCombined
                             ? "QBochs: reusing write-combined cache policy for this boot\n"
                             : "QBochs: reusing uncached cache policy for this boot\n"));
    }
    else
    {
        if (Status == NO_ERROR && DeviceExtension->WriteCombiningReady)
            DeviceExtension->BootCachePolicy = QBochsBootCachePolicyWriteCombined;
        else
            DeviceExtension->BootCachePolicy = QBochsBootCachePolicyUncached;

        if (!QBochsLatchBootCachePolicy(DeviceExtension))
        {
            /*
             * Without a volatile latch a reload in this same boot could change
             * cache attributes. Stay uncached and do not arm WC for next boot.
             */
            DeviceExtension->BootCachePolicy = QBochsBootCachePolicyUncached;
            DeviceExtension->WriteCombiningReady = FALSE;
            VideoDebugPrint((Error, "QBochs: volatile boot latch unavailable; forcing uncached framebuffer\n"));
        }
        else
        {
            VideoDebugPrint((Info,
                             DeviceExtension->BootCachePolicy == QBochsBootCachePolicyWriteCombined
                                 ? "QBochs: write combining enabled for this boot\n"
                                 : "QBochs: deferring write combining until after reboot\n"));
        }
    }

    if (!QBochsGetControllerInfo(DeviceExtension))
        return FALSE;

    return QBochsInitializeModes(DeviceExtension);
}

CODE_SEG("PAGE")
BOOLEAN NTAPI
QBochsStartIO(
    _In_ PVOID HwDeviceExtension,
    _Inout_ PVIDEO_REQUEST_PACKET RequestPacket)
{
    PQBOCHS_DEVICE_EXTENSION DeviceExtension = HwDeviceExtension;

    RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
    RequestPacket->StatusBlock->Information = 0;

    switch (RequestPacket->IoControlCode)
    {
        case IOCTL_VIDEO_MAP_VIDEO_MEMORY:
            if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY) ||
                RequestPacket->OutputBufferLength < sizeof(VIDEO_MEMORY_INFORMATION))
            {
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }
            return QBochsMapVideoMemory(DeviceExtension,
                                        (PVIDEO_MEMORY)RequestPacket->InputBuffer,
                                        (PVIDEO_MEMORY_INFORMATION)RequestPacket->OutputBuffer,
                                        RequestPacket->StatusBlock);

        case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY:
            if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY))
            {
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }
            return QBochsUnmapVideoMemory(DeviceExtension,
                                          (PVIDEO_MEMORY)RequestPacket->InputBuffer,
                                          RequestPacket->StatusBlock);

        case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:
            if (RequestPacket->OutputBufferLength < sizeof(VIDEO_NUM_MODES))
            {
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }
            return QBochsQueryNumAvailableModes(DeviceExtension,
                                                (PVIDEO_NUM_MODES)RequestPacket->OutputBuffer,
                                                RequestPacket->StatusBlock);

        case IOCTL_VIDEO_QUERY_AVAIL_MODES:
            if (RequestPacket->OutputBufferLength <
                DeviceExtension->AvailableModeCount * sizeof(VIDEO_MODE_INFORMATION))
            {
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }
            return QBochsQueryAvailableModes(DeviceExtension,
                                             (PVIDEO_MODE_INFORMATION)RequestPacket->OutputBuffer,
                                             RequestPacket->StatusBlock);

        case IOCTL_VIDEO_SET_CURRENT_MODE:
            if (RequestPacket->InputBufferLength < sizeof(VIDEO_MODE))
            {
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }
            return QBochsSetCurrentMode(DeviceExtension,
                                        (PVIDEO_MODE)RequestPacket->InputBuffer,
                                        RequestPacket->StatusBlock);

        case IOCTL_VIDEO_QUERY_CURRENT_MODE:
            if (RequestPacket->OutputBufferLength < sizeof(VIDEO_MODE_INFORMATION))
            {
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }
            return QBochsQueryCurrentMode(DeviceExtension,
                                          (PVIDEO_MODE_INFORMATION)RequestPacket->OutputBuffer,
                                          RequestPacket->StatusBlock);

        case IOCTL_VIDEO_RESET_DEVICE:
            RequestPacket->StatusBlock->Status = NO_ERROR;
            return TRUE;

        case IOCTL_VIDEO_GET_CHILD_STATE:
            if (RequestPacket->OutputBufferLength < sizeof(ULONG))
            {
                RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return FALSE;
            }
            *(PULONG)RequestPacket->OutputBuffer = VIDEO_CHILD_ACTIVE;
            RequestPacket->StatusBlock->Information = sizeof(ULONG);
            RequestPacket->StatusBlock->Status = NO_ERROR;
            return TRUE;
    }

    return FALSE;
}

CODE_SEG("PAGE")
VP_STATUS NTAPI
QBochsSetPowerState(
    _In_ PVOID HwDeviceExtension,
    _In_ ULONG HwId,
    _In_ PVIDEO_POWER_MANAGEMENT VideoPowerControl)
{
    return NO_ERROR;
}

CODE_SEG("PAGE")
VP_STATUS NTAPI
QBochsGetPowerState(
    _In_ PVOID HwDeviceExtension,
    _In_ ULONG HwId,
    _Out_ PVIDEO_POWER_MANAGEMENT VideoPowerControl)
{
    return ERROR_DEVICE_REINITIALIZATION_NEEDED;
}

CODE_SEG("PAGE")
VP_STATUS NTAPI
QBochsGetVideoChildDescriptor(
    _In_ PVOID HwDeviceExtension,
    _In_ PVIDEO_CHILD_ENUM_INFO ChildEnumInfo,
    _Out_ PVIDEO_CHILD_TYPE VideoChildType,
    _Out_ PUCHAR pChildDescriptor,
    _Out_ PULONG UId,
    _Out_ PULONG pUnused)
{
    PQBOCHS_DEVICE_EXTENSION DeviceExtension = HwDeviceExtension;

    if (ChildEnumInfo->Size < sizeof(*VideoChildType))
        return VIDEO_ENUM_NO_MORE_DEVICES;

    if (ChildEnumInfo->ChildIndex == 0)
        return VIDEO_ENUM_INVALID_DEVICE;

    *pUnused = 0;

    if (ChildEnumInfo->ChildIndex == DISPLAY_ADAPTER_HW_ID)
    {
        *VideoChildType = VideoChip;
        return VIDEO_ENUM_MORE_DEVICES;
    }

    if (ChildEnumInfo->ChildIndex != 1)
        return VIDEO_ENUM_NO_MORE_DEVICES;

    *UId = 0;
    *VideoChildType = Monitor;

    if (pChildDescriptor &&
        ChildEnumInfo->ChildDescriptorSize >= VBE_EDID_SIZE &&
        !DeviceExtension->IoPorts.RangeInIoSpace)
    {
        VideoPortMoveMemory(pChildDescriptor, DeviceExtension->IoPorts.Mapped, VBE_EDID_SIZE);
    }

    return VIDEO_ENUM_MORE_DEVICES;
}

ULONG NTAPI
DriverEntry(PVOID Context1, PVOID Context2)
{
    VIDEO_HW_INITIALIZATION_DATA VideoInitData;

    VideoPortZeroMemory(&VideoInitData, sizeof(VideoInitData));

    /*
     * Windows 2000 rejects the XP-sized structure with STATUS_REVISION_MISMATCH.
     * XP/Server 2003 remain backward-compatible with the Windows 2000 size.
     */
    VideoInitData.HwInitDataSize = SIZE_OF_W2K_VIDEO_HW_INITIALIZATION_DATA;
    VideoInitData.HwFindAdapter = QBochsFindAdapter;
    VideoInitData.HwInitialize = QBochsInitialize;
    VideoInitData.HwStartIO = QBochsStartIO;
    VideoInitData.HwDeviceExtensionSize = sizeof(QBOCHS_DEVICE_EXTENSION);
    VideoInitData.HwSetPowerState = QBochsSetPowerState;
    VideoInitData.HwGetPowerState = QBochsGetPowerState;
    VideoInitData.HwGetVideoChildDescriptor = QBochsGetVideoChildDescriptor;

    return VideoPortInitialize(Context1, Context2, &VideoInitData, NULL);
}
