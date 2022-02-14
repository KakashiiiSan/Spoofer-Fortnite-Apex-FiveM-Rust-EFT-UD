#pragma once

#include <iostream>
#include <Windows.h>
#include <functional>
#include <vector>

typedef struct _IMAGE_RELOCATION_ENTRY
{
	USHORT Offset : 12;
	USHORT Type : 4;
} IMAGE_RELOCATION_ENTRY, * PIMAGE_RELOCATION_ENTRY;

class PEImage
{
	std::vector<::byte> image;
	std::vector<::byte> mapped_image;
	PIMAGE_DOS_HEADER pDosHeader;
	PIMAGE_NT_HEADERS pNtHeaders;

	uintptr_t resolveRVA(uintptr_t rva);
public:

	PEImage(std::vector<::byte> image);
	void mapImage();
	void processRelocations(uintptr_t actual_base);
	void resolveImports(std::function<uintptr_t(const char*, const char*)> _get_import_address);
	void* getImage();
	void* getMappedImage();
	uint64_t getSize();
	uintptr_t getImageBase();
	uintptr_t getEntryPoint();
	PIMAGE_DOS_HEADER getDosHeader();
	PIMAGE_NT_HEADERS getNTHeaders();
};