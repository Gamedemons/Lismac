#include "gui.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_stdlib.h"
#include "../lib/tinyfiledialogs.h"
#include <string>
#include <regex>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <future>
#include "lance_utils.h"

using std::regex_replace;
using std::filesystem::directory_iterator;
using namespace std::chrono;
using std::string;
using std::to_string;
using std::vector;
using std::ifstream;
using std::ofstream;
using lance::getFileNames;
using lance::getCurrentTime;
using lance::handleUnicodeStrings;

#define IM_CLAMP(V, MN, MX)     ((V) < (MN) ? (MN) : (V) > (MX) ? (MX) : (V))
#define IM_NEWLINE  "\n"


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wideParameter, LPARAM longParameter
);

typedef struct RgbColor
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
} RgbColor;
typedef struct HsvColor
{
	unsigned char h;
	unsigned char s;
	unsigned char v;
} HsvColor;

long __stdcall WindowProcess(HWND window, UINT message, WPARAM wideParameter, LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE: {
		if (gui::device && wideParameter != SIZE_MINIMIZED)
		{
			gui::presentParameters.BackBufferWidth = LOWORD(longParameter);
			gui::presentParameters.BackBufferHeight = HIWORD(longParameter);
			gui::ResetDevice();
		}
	}return 0;

	case WM_SYSCOMMAND: {
		if ((wideParameter & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
	}break;

	case WM_DESTROY: {
		PostQuitMessage(0);
	}return 0;

	case WM_LBUTTONDOWN: {
		gui::position = MAKEPOINTS(longParameter); // set click points
	}return 0;

	case WM_MOUSEMOVE: {
		if (wideParameter == MK_LBUTTON)
		{
			const auto points = MAKEPOINTS(longParameter);
			auto rect = ::RECT{ };

			GetWindowRect(gui::window, &rect);

			rect.left += points.x - gui::position.x;
			rect.top += points.y - gui::position.y;

			if (gui::position.x >= 0 &&
				gui::position.x <= gui::WIDTH &&
				gui::position.y >= 0 && gui::position.y <= 19)
				SetWindowPos(
					gui::window,
					HWND_TOPMOST,
					rect.left,
					rect.top,
					0, 0,
					SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
				);
		}

	}return 0;

	}

	return DefWindowProc(window, message, wideParameter, longParameter);
}
void gui::CreateHWindow(const char* windowName) noexcept
{
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandleA(0);
	windowClass.hIcon = 0;
	windowClass.hCursor = 0;
	windowClass.hbrBackground = 0;
	windowClass.lpszMenuName = 0;
	windowClass.lpszClassName = "class001";
	windowClass.hIconSm = 0;

	RegisterClassEx(&windowClass);

	window = CreateWindowEx(
		0,
		"class001",
		windowName,
		WS_POPUP,
		100,
		100,
		WIDTH,
		HEIGHT,
		0,
		0,
		windowClass.hInstance,
		0
	);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
}
void gui::DestroyHWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}
bool gui::CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));

	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&presentParameters,
		&device) < 0)
		return false;

	return true;
}
void gui::ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}
void gui::DestroyDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}

	if (d3d)
	{
		d3d->Release();
		d3d = nullptr;
	}
}
void gui::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();

	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);
}
void gui::DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}
void gui::BeginRender() noexcept
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);

		if (message.message == WM_QUIT)
		{
			isRunning = !isRunning;
			return;
		}
	}

	// Start the Dear ImGui frame
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}
void gui::EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}

	const auto result = device->Present(0, 0, 0, 0);

	// Handle loss of D3D9 device
	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
}
void gui::Render() noexcept
{
	// Static 
	static bool show_app_style_editor = false;
	static bool show_app_settings = false;

	static short int xPos = 10;
	static short int yPos = 60;
	static short int lGap = 35;

	static int tab_stack = 0;

	static bool enable_markdown_output = true;
	static bool recursive_search = false;
	static bool enable_overwrite_mode = true;
	static bool enable_heading = true;
	static bool enable_fullpaths = false;
	static bool enable_tct = true;


	// Global Style Vars
	static int theme = 1;
	ImGuiStyle& style = ImGui::GetStyle();
	{
		style.FrameRounding = 3.0f;
		style.WindowPadding = ImVec2(0, 0);
		style.FramePadding = ImVec2(4.0, 6.0);
		style.GrabRounding = 1.0;
		//style.ScrollbarRounding = 7.0;
		if (theme == 0)	// Default Theme
		{
			style.Colors[ImGuiCol_Text] = ImColor(255, 255, 255, 255);
			style.Colors[ImGuiCol_TextDisabled] = ImColor(128, 128, 128, 255);
			style.Colors[ImGuiCol_WindowBg] = ImColor(15, 15, 15, 240);
			style.Colors[ImGuiCol_PopupBg] = ImColor(20, 20, 20, 240);
			style.Colors[ImGuiCol_FrameBg] = ImColor(41, 74, 122, 138);
			style.Colors[ImGuiCol_TitleBg] = ImColor(10, 10, 10, 255);
			style.Colors[ImGuiCol_TitleBgActive] = ImColor(41, 74, 122, 255);
			style.Colors[ImGuiCol_ScrollbarBg] = ImColor(5, 5, 5, 135);
			style.Colors[ImGuiCol_ScrollbarGrab] = ImColor(79, 79, 79, 255);
			style.Colors[ImGuiCol_Tab] = ImColor(46, 89, 148, 220);
			style.Colors[ImGuiCol_TabActive] = ImColor(51, 105, 173, 255);
			style.Colors[ImGuiCol_PlotHistogram] = ImColor(230, 179, 0);
		}
		else if (theme == 1) // Dracula Theme
		{
			style.Colors[ImGuiCol_Text] = ImColor(255, 255, 255, 255);
			style.Colors[ImGuiCol_TextDisabled] = ImColor(128, 128, 128, 255);
			style.Colors[ImGuiCol_WindowBg] = ImColor(37, 37, 37);
			style.Colors[ImGuiCol_PopupBg] = ImColor(20, 20, 20, 240);
			style.Colors[ImGuiCol_FrameBg] = ImColor(41, 74, 122, 138);
			style.Colors[ImGuiCol_TitleBg] = ImColor(10, 10, 10, 255);
			style.Colors[ImGuiCol_TitleBgActive] = ImColor(41, 74, 122, 255);
			style.Colors[ImGuiCol_ScrollbarBg] = ImColor(5, 5, 5, 135);
			style.Colors[ImGuiCol_ScrollbarGrab] = ImColor(79, 79, 79, 255);
			style.Colors[ImGuiCol_Tab] = ImColor(46, 89, 148, 220);
			style.Colors[ImGuiCol_TabActive] = ImColor(51, 105, 173, 255);
			style.Colors[ImGuiCol_PlotHistogram] = ImColor(117, 255, 121);
		}
		else if (theme == 2) // Violet Candy
		{
			style.Colors[ImGuiCol_Text] = ImColor(0, 0, 0, 255);
			style.Colors[ImGuiCol_TextDisabled] = ImColor(55, 55, 55, 255);
			style.Colors[ImGuiCol_WindowBg] = ImColor(211, 208, 255, 255);
			style.Colors[ImGuiCol_PopupBg] = ImColor(215, 203, 255, 255);
			style.Colors[ImGuiCol_FrameBg] = ImColor(255, 255, 255, 255);
			style.Colors[ImGuiCol_TitleBg] = ImColor(122, 102, 255, 255);
			style.Colors[ImGuiCol_TitleBgActive] = ImColor(177, 129, 255, 255);
			style.Colors[ImGuiCol_ScrollbarBg] = ImColor(255, 255, 255, 0);
			style.Colors[ImGuiCol_ScrollbarGrab] = ImColor(89, 89, 89, 255);
			style.Colors[ImGuiCol_Tab] = ImColor(131, 184, 255, 220);
			style.Colors[ImGuiCol_TabActive] = ImColor(104, 133, 255, 255);
			style.Colors[ImGuiCol_PlotHistogram] = ImColor(132, 90, 255, 255);
		}
	}

	// Window Settings
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowSize({ WIDTH, HEIGHT }); // 1280 x 720
	ImGui::Begin(
		"Lismac",
		&isRunning,
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoBringToFrontOnFocus
	);

	// Code here -------------------------------------------------------------------------------------------------------------------------|


	// Frame Rate Label - 6 for half of 24 (button height)
	ImGui::SetCursorPos(ImVec2(
		lance::toShint(WIDTH - 200.0),
		30 + 0
	));
	ImGui::Text("RT : %.1f ms/fr (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);


	// Task Completion Label - 6 for half of 24 (button height)
	static long taskCompletionTime = 0;
	ImGui::SetCursorPos(ImVec2(
		lance::toShint(WIDTH - 200.0),
		30 + 12
	));
	ImGui::Text("CT : %d ms", taskCompletionTime);


	// Style Editor
	ImGui::SetCursorPos(ImVec2(
		lance::toShint(WIDTH - 300.0),
		30
	));
	if (ImGui::Button("Style Editor", ImVec2(90, 25))) {
		show_app_style_editor = !show_app_style_editor;
	}
	if (show_app_style_editor)
	{
		ImGui::Begin("Style Editor", &show_app_style_editor);
		ImGui::ShowStyleEditor();
		ImGui::End();
	}


	// Settings
	ImGui::SetCursorPos(ImVec2(
		lance::toShint(WIDTH - 400.0),
		30
	));
	if (ImGui::Button("Settings", ImVec2(90, 25))) {
		show_app_settings = true;
	}
	if (show_app_settings)
	{
		ImGui::SetNextWindowPos({ 0, 0 });
		ImGui::SetNextWindowSize({ WIDTH, HEIGHT }); // 1280 x 720
		ImGui::Begin(
			"Lance Settings",
			&show_app_settings,
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoCollapse
		);


		ImGui::End();
	}


	// Theme
	const char* themeType[] = { "No Theme", "Drakula", "Violet Candy" };
	ImGui::SetCursorPos(ImVec2(
		lance::toShint(WIDTH - 600.0),
		30
	));
	ImGui::PushItemWidth(190);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 5));
	ImGui::Combo("##themeCombo", &theme, themeType, IM_ARRAYSIZE(themeType));
	ImGui::PopStyleVar();



	// Tabs
	ImGui::SetCursorPos(ImVec2(xPos, yPos));
	if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None))
	{
		// Home Tab
		if (ImGui::BeginTabItem("Home - Configuration")) {

			// File Picker
			static char inputLocation[50000] = "D:\\Z";

			ImGui::SetCursorPos(ImVec2(
				lance::toShint(xPos),
				lance::toShint(yPos + (lGap * 1.0))
			));
			ImGui::Text("Input Folder");

			ImGui::SetCursorPos(ImVec2(
				lance::toShint(xPos),
				lance::toShint(yPos + (lGap * 1.5))
			));
			ImGui::PushItemWidth(WIDTH/2 - xPos - 45);
			ImGui::InputText("##inputLocationLabel", inputLocation, IM_ARRAYSIZE(inputLocation), ImGuiInputTextFlags_ReadOnly);
			ImGui::SameLine();
			ImGui::SetCursorPos(ImVec2(
				WIDTH/2 - 35,
				yPos + (lGap * 1.5)
			));
			if (ImGui::Button("...##inputLocationButton1", ImVec2(25, 25))) {
				try {
					char* res = tinyfd_selectFolderDialog("Pick Input Folder", NULL);
					if (res == NULL) {
						throw "Invalid Path";
					}
					strcpy_s(inputLocation, res);
				}
				catch (...) {
				}
			}
			
			// Output Picker
			static char outputLocation[50000] = "C:\\Users\\GameDemons\\Documents\\Obsidian\\Obsidian Default\\Rough Work";

			ImGui::SetCursorPos(ImVec2(
				lance::toShint(xPos),
				lance::toShint(yPos + (lGap * 2.5))
			));
			ImGui::Text("Output Folder");

			ImGui::SetCursorPos(ImVec2(
				xPos, 
				yPos + (lGap * 3.0)
			));
			ImGui::PushItemWidth(WIDTH / 2 - xPos - 45);
			ImGui::InputText("##outputLocationLabel", outputLocation, IM_ARRAYSIZE(outputLocation), ImGuiInputTextFlags_ReadOnly);
			ImGui::SameLine();
			ImGui::SetCursorPos(ImVec2(
				WIDTH / 2 - 35,
				yPos + (lGap * 3.0)
			));
			if (ImGui::Button("...##outputLocationButton", ImVec2(25, 25))) {
				try {
					char* res = tinyfd_selectFolderDialog("Pick Output Folder", "");
					if (res == NULL) {
						throw "Invalid Path";
					}
					strcpy_s(outputLocation, res);
				}
				catch (...) {
				}
			}


			// Output File Name
			static char outputFileName[50000] = "#lismac";

			ImGui::SetCursorPos(ImVec2(
				lance::toShint(xPos),
				lance::toShint(yPos + (lGap * 4.0))
			));
			ImGui::Text("Output File Name");

			ImGui::SetCursorPos(ImVec2(
				xPos,
				yPos + (lGap * 4.5)
			));
			ImGui::PushItemWidth(WIDTH / 2 - xPos - 45);
			ImGui::InputText("##outputFileName", outputFileName, IM_ARRAYSIZE(outputFileName));


			// Enable Markdown Output
			ImGui::SetCursorPos(ImVec2(
				lance::toShint(xPos),
				lance::toShint(yPos + (lGap * 5.5))
			));
			ImGui::Checkbox("Enable Markdown Output (Recommended : Enabled)##enable_markdown_output", &enable_markdown_output);

			// Include Sub Folders
			ImGui::SetCursorPos(ImVec2(
				lance::toShint(xPos),
				lance::toShint(yPos + (lGap * 6.5))
			));
			ImGui::Checkbox("Include Sub Folders (Recursive Search)##recursive_search", &recursive_search);

			// Enable Overwrite Mode
			ImGui::SetCursorPos(ImVec2(
				lance::toShint(xPos),
				lance::toShint(yPos + (lGap * 7.5))
			));
			ImGui::Checkbox("Enable Overwrite Mode##enable_overwrite_mode", &enable_overwrite_mode);

			// Enable Heading
			ImGui::SetCursorPos(ImVec2(
				lance::toShint(xPos),
				lance::toShint(yPos + (lGap * 8.5))
			));
			ImGui::Checkbox("Enable Heading##enable_heading", &enable_heading);

			// Enable Full Paths
			ImGui::SetCursorPos(ImVec2(
				lance::toShint(xPos),
				lance::toShint(yPos + (lGap * 9.5))
			));
			ImGui::Checkbox("Use Full Paths##enable_fullpaths", &enable_fullpaths);

			// Enable Task Completion Time
			ImGui::SetCursorPos(ImVec2(
				lance::toShint(xPos),
				lance::toShint(yPos + (lGap * 10.5))
			));
			ImGui::Checkbox("Enable Task Completion Time##enable_tct", &enable_tct);



			// Output Button
			ImGui::SetCursorPos(ImVec2(
				lance::toShint(xPos),
				lance::toShint(yPos + (lGap * 17.0) + 2)
			));
			if (ImGui::Button("Generate Output", ImVec2(WIDTH - xPos - 10, 25 * 2)))
			{
				/*string time = "";*/
				long ms1 = lance::getCurrentTime('m');
				
				std::string outputFileMaker = lance::getOutputFileName(outputLocation, outputFileName, enable_markdown_output);
				std::string seperator = "\n";

				auto writeType = std::ios::app;
				if (enable_overwrite_mode == true) {
					writeType = std::ios::trunc;
				}
				std::ofstream outputFile(outputFileMaker, writeType);
				outputFile << "\n";

				// Heading
				std::string heading = "List of Contents - ";
				if (enable_markdown_output == true) {
					heading = "## " + heading;
				}
				heading += inputLocation;
				outputFile << heading + "\n\n";


				lance::writeFile(
					outputFile,
					inputLocation,
					seperator,
					tab_stack
				);


				long ms2 = lance::getCurrentTime('m');
				taskCompletionTime = ms2 - ms1;
				if (enable_tct == true) {
					outputFile << seperator << "\n\nCompletion Time : " << to_string(ms2 - ms1) << "ms" << seperator;
				}

				outputFile.close();
			}

			ImGui::EndTabItem();
		}

		// Logs Tab
		if (ImGui::BeginTabItem("Logs")) {
			ImGui::EndTabItem();
		}

		// Help Tab
		if (ImGui::BeginTabItem("Help")) {
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	// Code Ends-----------------------------------------------------------------------------------------------------------------------|
	ImGui::End();
}


// Lance Functions
void lance::writeFile(
	std::ofstream& outputFile,
	char inputLocation[],
	std::string seperator,
	int& tab_stack
) 
{
	auto dirIter = directory_iterator(inputLocation);

	std::string tabs = lance::getTabs(tab_stack);
	
	try {
		for (const auto& file : dirIter) {
			if (file.is_directory()) 
			{
				// Writing Folder Name
				const std::wstring currentPath = file.path().c_str();
				string foldername = lance::handleUnicodeStrings(currentPath);
				std::filesystem::path pp(foldername);
				
				foldername = pp.parent_path().filename().string() + "   --   " + lance::doRegex(foldername, "\\\\", "\\\\");
				if (!outputFile.fail())
				{
					outputFile << tabs << "- " + foldername + "" << seperator;
				}


				std::filesystem::path p(file.path());
				string newLocation = p.string();
				char newArr[50000] = "";
				strcpy_s(newArr, newLocation.c_str());


				lance::increaseStack(tab_stack);
				lance::writeFile(
					outputFile,
					newArr,
					seperator,
					tab_stack
				);
				lance::decreaseStack(tab_stack);
				continue;
			}

			const std::wstring currentPath = file.path().c_str();
			string filename = lance::handleUnicodeStrings(currentPath);
			filename = lance::extractName(filename, false);
			if (!outputFile.fail())
			{
				outputFile << tabs << "`" + filename + "`" << seperator;
			}
		}
	}
	catch (const std::exception& e) {
		outputFile << seperator << e.what() << seperator;
	}
};

vector<string> lance::getFileNames(char path[]) {
	auto dirIter = directory_iterator(path);
	vector<string> files;

	for (const auto& file : dirIter) {
		/*if (!file.is_directory()) {*/
			const std::wstring currentPath = file.path().c_str();
			string output = lance::handleUnicodeStrings(currentPath);
			files.push_back(output);
		//}
	}

	return files;
}

long long lance::getFileSize(string path) {
	return std::filesystem::file_size(path);
};

long lance::getCurrentTime(char type) {
	milliseconds time = duration_cast<milliseconds>(
		system_clock::now().time_since_epoch()
	);

	if (type == 's') {
		return (long)(time.count() / 1000);
	}

	return (long)(time.count());
}

std::string lance::getOutputFileName(char path[], char name[], bool enable_markdown) {
	std::string part1 = std::string(path) + "\\" + std::string(name);
	if (enable_markdown == true) return part1 + ".md";
	return part1 + ".txt";
}

std::string lance::extractName(std::string str, bool pathVisible)
{
	if (pathVisible) return str;
	std::filesystem::path p(str);
	return p.filename().string();
}

std::string lance::doRegex(std::string str, std::string reg1, std::string reg2) {
	return std::regex_replace(str, std::regex(reg1), reg2);
}
std::string lance::doRegexForSlash(std::string str) {
	return std::regex_replace(str, std::regex("\\s*"), "\\\\");
}

std::string lance::getTabs(int stack) {
	string tabs = "";
	for (int i = stack; i > 0; i--) {
		tabs += "\t";
	}
	return tabs;
}

// Type Conversions
short int lance::toShint(short int x)
{
	return (short int)(x);
}
short int lance::toShint(double x)
{
	return (short int)(x);
}
float lance::toFloat(double x)
{
	return (float)(x);
}
std::string lance::handleUnicodeStrings(const std::wstring& wide_string) {
	if (wide_string.empty())
	{
		return "";
	}

	const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, &wide_string.at(0), (int)wide_string.size(), nullptr, 0, nullptr, nullptr);
	if (size_needed <= 0)
	{
		throw std::runtime_error("WideCharToMultiByte() failed: " + std::to_string(size_needed));
	}

	std::string result(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wide_string.at(0), (int)wide_string.size(), &result.at(0), size_needed, nullptr, nullptr);
	return result;
}

void lance::increaseStack(int& stack){
	++stack;
};
void lance::decreaseStack(int& stack){
	--stack;
};


void lance::ltrim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
		return !std::isspace(ch);
		}));
}
void lance::rtrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
		return !std::isspace(ch);
		}).base(), s.end()
			);
}
void lance::trim(std::string& s) {
	rtrim(s);
	ltrim(s);
}

bool lance::isNumber(const std::string s) {
	char sign = s.at(0);
	bool allNumbers = s.find_first_not_of("0123456789") == string::npos;
	if (sign == '+' || sign == '-') {
		string substring = s.substr(1);
		return substring.find_first_not_of("0123456789") == string::npos;
	}

	return allNumbers;
}