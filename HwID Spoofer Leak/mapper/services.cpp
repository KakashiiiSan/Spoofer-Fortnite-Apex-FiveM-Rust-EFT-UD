#include <fstream>
#include <ostream>
#include <iostream>
#include "services.hpp";
#include "intel.hpp"
#include "..\Crypter.hpp"

DriverLoader::DriverLoader(std::wstring name)
{
	service_name = name;
}

bool DriverLoader::create_file_path(char* buffer, size_t size)
{
	wchar_t temp_dir[MAX_PATH + 1];
	GetTempPathW(MAX_PATH, temp_dir);

	file_path = std::wstring(temp_dir) + service_name + EncryptWS(L".sys");
	std::ofstream out_file(file_path, std::ios::binary);
	out_file.write(buffer, size);
	out_file.close();

	return true;
}

bool DriverLoader::delete_file()
{
	std::string path(file_path.begin(), file_path.end());

	return DeleteFileA(path.c_str());
}

bool DriverLoader::create_service_reg_key()
{
	HKEY services_key;
	HKEY intel_key;
	DWORD type = 1;
	DWORD control = 0;
	DWORD start = 3;
	std::string path_name(file_path.begin(), file_path.end());
	std::string image_path = EncryptS("\\??\\") + path_name;
	std::string name(service_name.begin(), service_name.end());

	if (RegOpenKeyA(HKEY_LOCAL_MACHINE, EncryptS("SYSTEM\\CurrentControlSet\\Services"), &services_key) != ERROR_SUCCESS)
		return false;

	if (RegCreateKeyW(services_key, service_name.c_str(), &intel_key) != ERROR_SUCCESS)
		return false;

	if (RegSetValueExA(intel_key, EncryptS("ImagePath"), 0, REG_EXPAND_SZ, (BYTE*)image_path.c_str(), image_path.length()) != ERROR_SUCCESS)
		return false;

	if (RegSetValueExA(intel_key, EncryptS("Type"), 0, REG_DWORD, (BYTE*)&type, sizeof(type)) != ERROR_SUCCESS)
		return false;

	RegCloseKey(services_key);
	RegCloseKey(intel_key);
	return true;
}

bool DriverLoader::delete_service_reg_key()
{
	HKEY services_key;
	if (RegOpenKeyA(HKEY_LOCAL_MACHINE, EncryptS("SYSTEM\\CurrentControlSet\\Services"), &services_key) != ERROR_SUCCESS)
		return false;

	bool success = RegDeleteTreeW(services_key, service_name.c_str()) == ERROR_SUCCESS;
	RegCloseKey(services_key);

	return success;
}

bool DriverLoader::escalate_privilege()
{
	LUID luid;
	HANDLE token;
	TOKEN_PRIVILEGES tp;
	if (!LookupPrivilegeValueA(NULL, EncryptS("SeLoadDriverPrivilege"), &luid))
		return false;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
		return false;

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	if (!AdjustTokenPrivileges(token, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL))
		return false;

	CloseHandle(token);

	return true;
}

bool DriverLoader::load_driver(char* buffer, size_t size)
{
	if (!create_file_path(buffer, size))
	{
		return false;
	}

	if (!create_service_reg_key())
	{
		return false;
	}

	if (!escalate_privilege())
	{
		return false;
	}

	std::wstring driver_service = EncryptWS(L"\\registry\\machine\\SYSTEM\\CurrentControlSet\\Services\\") + service_name;
	UNICODE_STRING driver_service_name;
	RtlInitUnicodeString(&driver_service_name, driver_service.c_str());

	return NT_SUCCESS(nt::NtLoadDriver(&driver_service_name));
}

bool DriverLoader::unload_driver()
{
	std::wstring driver_service = EncryptWS(L"\\registry\\machine\\SYSTEM\\CurrentControlSet\\Services\\") + service_name;
	UNICODE_STRING driver_service_name;
	RtlInitUnicodeString(&driver_service_name, driver_service.c_str());

	return NT_SUCCESS(nt::NtUnloadDriver(&driver_service_name));
}

bool DriverLoader::clean_up()
{
	return delete_service_reg_key() && delete_file();
}