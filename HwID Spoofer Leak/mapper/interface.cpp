#include "interface.hpp"
#include "..\Crypter.hpp"

KeInterface::KeInterface() 
{
	hDriver = CreateFileW(EncryptWS(L"\\\\.\\Nal"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	if (hDriver == INVALID_HANDLE_VALUE)
	{
		exit(0);
	}
}

uintptr_t KeInterface::getKernelModuleBase(const char* name)
{
	NTSTATUS status;
	DWORD bytes = 0;
	std::vector<byte> buffer;

	while ((status = NtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)nt::SystemModuleInformation, buffer.data(), buffer.size(), &bytes)) == nt::STATUS_INFO_LENGTH_MISMATCH)
	{
		buffer.resize(bytes);
	}

	if (!NT_SUCCESS(status))
		return 0;

	nt::PSYSTEM_MODULE_INFORMATION pSystemModuleInfo = nt::PSYSTEM_MODULE_INFORMATION(buffer.data());
	nt::PSYSTEM_MODULE pModule = pSystemModuleInfo->Modules;
	for (int i = 0; i < pSystemModuleInfo->NumberOfModules; i++, pModule++)
	{
		PCHAR moduleName = PCHAR(uintptr_t(pModule->FullPathName) + pModule->OffsetToFileName);
		if (!strcmp(name, moduleName))
			return uintptr_t(pModule->ImageBase);
	}

	return 0;
}

uintptr_t KeInterface::getModuleExport(const char* module_name, uintptr_t module_base)
{
	IMAGE_DOS_HEADER dosHeader = rm<IMAGE_DOS_HEADER>(module_base);
	if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
		return 0;

	IMAGE_NT_HEADERS ntHeaders = rm<IMAGE_NT_HEADERS>(module_base + dosHeader.e_lfanew);
	if (ntHeaders.Signature != IMAGE_NT_SIGNATURE || ntHeaders.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
		return 0;

	IMAGE_DATA_DIRECTORY exportTableDir = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	void* pExportTable = VirtualAlloc(NULL, exportTableDir.Size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	PIMAGE_EXPORT_DIRECTORY pExportDir = PIMAGE_EXPORT_DIRECTORY(pExportTable);

	writeMemory(module_base + exportTableDir.VirtualAddress, (uintptr_t)pExportTable, exportTableDir.Size);
	if (!pExportDir)
		return 0;

	uintptr_t offset = uintptr_t(pExportTable) - exportTableDir.VirtualAddress;
	PDWORD pName = PDWORD(pExportDir->AddressOfNames + offset);
	PWORD pOrdinalTable = PWORD(pExportDir->AddressOfNameOrdinals + offset);
	PDWORD pAddressTable = PDWORD(pExportDir->AddressOfFunctions + offset);
	for (size_t i = 0; i < pExportDir->NumberOfNames; i++, pName++)
	{
		PCHAR pModuleName = PCHAR(*pName + offset);
		if (!strcmp(module_name, pModuleName))
		{
			uintptr_t functionAddress = pAddressTable[pOrdinalTable[i]];
			if (functionAddress >= exportTableDir.VirtualAddress && functionAddress <= (uintptr_t)exportTableDir.VirtualAddress + exportTableDir.Size)
			{
				VirtualFree(pExportTable, 0, MEM_RELEASE);
				return 0;
			}
			VirtualFree(pExportTable, 0, MEM_RELEASE);
			return module_base + functionAddress;
		}
	}

	VirtualFree(pExportTable, 0, MEM_RELEASE);

	return 0;
}

bool KeInterface::writeMemory(uintptr_t source, uintptr_t destination, uint64_t size)
{
	if (hDriver == INVALID_HANDLE_VALUE)
		return false;

	WriteRequest req;
	req.source = source;
	req.destination = destination;
	req.size = size;
	DWORD bytes;

	return DeviceIoControl(hDriver, iotctl, &req, sizeof(req), nullptr, 0, &bytes, 0);
}

bool KeInterface::getPhysicalMemory(uintptr_t virtual_address, uintptr_t* out_physical_address)
{
	if (hDriver == INVALID_HANDLE_VALUE)
		return false;

	GetPhysicalRequest req;
	req.va = virtual_address;

	DWORD bytes;
	if (!DeviceIoControl(hDriver, iotctl, &req, sizeof(req), nullptr, 0, &bytes, 0))
		return false;

	*out_physical_address = req.pa;

	return true;
}

bool KeInterface::mapIoSpace(uintptr_t physical_address, uint32_t size, uintptr_t* out_virtual_address)
{
	if (hDriver == INVALID_HANDLE_VALUE)
		return false;

	MapIoRequest req;
	req.in_pa = physical_address;
	req.size = size;

	DWORD bytes;
	if (!DeviceIoControl(hDriver, iotctl, &req, sizeof(req), nullptr, 0, &bytes, 0))
		return false;

	*out_virtual_address = req.out_base_va;

	return true;
}

bool KeInterface::unMapIoSpace(uintptr_t base_virtual_address, uint32_t size)
{
	if (hDriver == INVALID_HANDLE_VALUE)
		return false;

	UnMapIoRequest req;
	req.in_base_va = base_virtual_address;
	req.size = size;
	DWORD bytes;

	return DeviceIoControl(hDriver, iotctl, &req, sizeof(req), nullptr, 0, &bytes, 0);
}

bool KeInterface::writeReadOnlyMemory(uintptr_t source, uintptr_t destination, uintptr_t size)
{
	uintptr_t physical_address;

	if (!getPhysicalMemory(destination, &physical_address))
		return false;

	uintptr_t virtual_address;

	if (!mapIoSpace(physical_address, size, &virtual_address))
		return false;

	bool result = writeMemory(source, virtual_address, size);

	if (!unMapIoSpace(virtual_address, size))
		return false;

	return result;
}

bool KeInterface::cleanUp()
{
	return CloseHandle(hDriver);
}