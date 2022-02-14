#pragma once
#include "nt.hpp"
#include <string>

class DriverLoader
{
	std::wstring service_name;
	std::wstring file_path;

	bool create_file_path(char* buffer, size_t size);
	bool delete_file();
	bool create_service_reg_key();
	bool delete_service_reg_key();
	bool escalate_privilege();
public:

	DriverLoader(std::wstring service_name);
	bool load_driver(char* buffer, size_t size);
	bool unload_driver();
	bool clean_up();
};