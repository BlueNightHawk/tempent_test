#include <Windows.h>
#include <string>

std::string g_ClDllPath;

extern bool InitClientDll();
extern void ShutdownClientDll();

BOOL WINAPI DllMain(
	HINSTANCE hinstDLL,
	DWORD fdwReason,
	LPVOID lpvReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
	{
		char filename[MAX_PATH];

		GetModuleFileName(hinstDLL, filename, MAX_PATH);
		g_ClDllPath = filename;

		auto start = g_ClDllPath.find("client.dll");
		g_ClDllPath.erase(start);
		g_ClDllPath.insert(start, "orig_client.dll");

		return InitClientDll();
	}
		break;

	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:

		if (lpvReserved != nullptr)
		{
			break;
		}
		ShutdownClientDll();

		break;
	}
	return TRUE;
}
