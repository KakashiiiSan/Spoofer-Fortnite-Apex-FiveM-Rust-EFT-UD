#include "pe.hpp"
#include "..\Crypter.hpp"

PEImage::PEImage(std::vector<byte> image)
{
	PEImage::image = image;
	pDosHeader = PIMAGE_DOS_HEADER(getImage());
	pNtHeaders = PIMAGE_NT_HEADERS(uintptr_t(getImage()) + pDosHeader->e_lfanew);
	if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE || pNtHeaders->Signature != IMAGE_NT_SIGNATURE || pNtHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
		std::cout << EncryptS(" ") << std::endl;
		exit(0);
	}
}

void PEImage::mapImage()
{
	mapped_image.clear();
	mapped_image.resize(getSize());
	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
	for (size_t i = 0; i < pNtHeaders->FileHeader.NumberOfSections; i++, pSectionHeader++)
		if (~pSectionHeader->Characteristics & IMAGE_SCN_MEM_DISCARDABLE)
			std::copy_n(image.begin() + uintptr_t(pSectionHeader->PointerToRawData), pSectionHeader->SizeOfRawData, mapped_image.begin() + uintptr_t(pSectionHeader->VirtualAddress));
}

void PEImage::processRelocations(uintptr_t actual_base)
{
	IMAGE_DATA_DIRECTORY relocation_table = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
	uintptr_t relocation_table_offset = resolveRVA(relocation_table.VirtualAddress);
	uintptr_t offset = 0;
	ptrdiff_t image_base_delta = actual_base - getImageBase();
	if (image_base_delta == 0 || relocation_table.Size == 0)
		return;

	while (offset < relocation_table.Size)
	{
		PIMAGE_BASE_RELOCATION pBaseReloc = PIMAGE_BASE_RELOCATION(uintptr_t(getImage()) + relocation_table_offset + offset);
		uintptr_t relocation_offset = resolveRVA(pBaseReloc->VirtualAddress);
		offset += sizeof(IMAGE_BASE_RELOCATION);
		size_t entries = (pBaseReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
		PIMAGE_RELOCATION_ENTRY pRelocationEntry = PIMAGE_RELOCATION_ENTRY(uintptr_t(pBaseReloc) + sizeof(IMAGE_BASE_RELOCATION));
		for (size_t i = 0; i < entries; i++, pRelocationEntry++)
		{
			offset += sizeof(IMAGE_RELOCATION_ENTRY);
			uintptr_t raw_address = uintptr_t(getImage()) + relocation_offset + pRelocationEntry->Offset;
			switch (pRelocationEntry->Type)
			{
			case IMAGE_REL_BASED_HIGH:
			{
				PSHORT pRelocationAddress = PSHORT(raw_address);
				*pRelocationAddress += HIWORD(image_base_delta);
				break;
			}
			case IMAGE_REL_BASED_LOW:
			{
				PSHORT pRelocationAddress = PSHORT(raw_address);
				*pRelocationAddress += LOWORD(image_base_delta);
				break;
			}
			case IMAGE_REL_BASED_HIGHLOW:
			{
				PDWORD64 pRelocationAddress = PDWORD64(raw_address);
				*pRelocationAddress += DWORD64(image_base_delta);
				break;
			}
			case IMAGE_REL_BASED_DIR64:
			{
				UNALIGNED PDWORD64 pRelocationAddress = PDWORD64(raw_address);
				*pRelocationAddress += image_base_delta;
				break;
			}
			case IMAGE_REL_BASED_ABSOLUTE:
				break;
			default:
			{
				continue;
			}
			}
		}
	}
}

void PEImage::resolveImports(std::function<uintptr_t(const char*, const char*)> _get_import_address)
{
	IMAGE_DATA_DIRECTORY relocation_table = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	uintptr_t import_table_offset = resolveRVA(relocation_table.VirtualAddress);
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = PIMAGE_IMPORT_DESCRIPTOR(uintptr_t(getImage()) + import_table_offset);
	for (; pImportDesc->Name; pImportDesc++)
	{
		uintptr_t import_name_offset = resolveRVA(pImportDesc->Name);
		PCHAR pImportName = PCHAR(uintptr_t(getImage()) + import_name_offset);
		PIMAGE_THUNK_DATA pThunkData;
		PIMAGE_THUNK_DATA pFirstThunk = PIMAGE_THUNK_DATA(uintptr_t(getImage()) + resolveRVA(pImportDesc->FirstThunk));
		if (pImportDesc->OriginalFirstThunk)
		{
			pThunkData = PIMAGE_THUNK_DATA(uintptr_t(getImage()) + resolveRVA(pImportDesc->OriginalFirstThunk));
		}
		else
		{
			pThunkData = pFirstThunk;
		}

		for (; pThunkData->u1.AddressOfData; pThunkData++, pFirstThunk++)
		{
			if (~pThunkData->u1.Ordinal & IMAGE_ORDINAL_FLAG)
			{
				PIMAGE_IMPORT_BY_NAME pThunkName = PIMAGE_IMPORT_BY_NAME(uintptr_t(getImage()) + resolveRVA(pThunkData->u1.AddressOfData));
				uintptr_t functionAddress = _get_import_address(pImportName, pThunkName->Name);
				pFirstThunk->u1.Function = functionAddress;
			}
			else
			{
				exit(0);
			}
		}
	}
}


void* PEImage::getImage()
{
	return image.data();
}

void* PEImage::getMappedImage()
{
	return mapped_image.data();
}

uint64_t PEImage::getSize()
{
	return pNtHeaders->OptionalHeader.SizeOfImage;
}

uintptr_t PEImage::getImageBase()
{
	return pNtHeaders->OptionalHeader.ImageBase;
}

uintptr_t PEImage::getEntryPoint()
{
	return pNtHeaders->OptionalHeader.AddressOfEntryPoint;
}

PIMAGE_DOS_HEADER PEImage::getDosHeader()
{
	return pDosHeader;
}

PIMAGE_NT_HEADERS PEImage::getNTHeaders()
{
	return pNtHeaders;
}

uintptr_t PEImage::resolveRVA(uintptr_t rva)
{
	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
	for (int i = 0; i < pNtHeaders->FileHeader.NumberOfSections - 1; i++, pSectionHeader++)
		if (rva < (pSectionHeader + 1)->VirtualAddress)
			break;

	return rva - pSectionHeader->VirtualAddress + pSectionHeader->PointerToRawData;
}