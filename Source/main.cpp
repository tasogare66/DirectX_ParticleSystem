#include <Windows.h>
#include <WindowsX.h>
#include "WinWindow.h"
#include "ParticleSystem.h"
#include "Timer.h"
#include "Input.h"
#include "IniParser.h"
//#include <DxErr.h>

#include <Poco/Mutex.h>
#include <Poco/AtomicCounter.h>
#include <Poco/File.h>
#include <Poco/URI.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>

struct wdmConfig
{
	uint16_t port;
	uint16_t max_queue;
	uint16_t max_threads;
	uint32_t json_reserve_size;
	bool disabled;

	wdmConfig()
		: port(10002)
		, max_queue(100)
		, max_threads(2)
		, json_reserve_size(1024 * 1024)
		, disabled(false)
	{}
};

class WuiSystem
{
public:
	WuiSystem();
	~WuiSystem()
	{

	}

private:
	wdmConfig m_conf;
	Poco::Net::HTTPServer *m_server;
};

struct MIME { const char *ext; const char *type; };
static const MIME s_mime_types[] = {
	{ ".txt",  "text/plain" },
	{ ".html", "text/html" },
	{ ".css",  "text/css" },
	{ ".js",   "text/javascript" },
	{ ".png",  "image/png" },
	{ ".jpg",  "image/jpeg" },
};

inline size_t GetModulePath(char *out_path, size_t len)
{
	HMODULE mod = 0;
	::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)&GetModulePath, &mod);
	DWORD size = ::GetModuleFileNameA(mod, out_path, (DWORD)len);
	return size;
}

inline bool GetModuleDirectory(char *out_path, size_t len)
{
	size_t size = GetModulePath(out_path, len);
	while (size>0) {
		if (out_path[size] == '\\') {
			out_path[size + 1] = '\0';
			return true;
		}
		--size;
	}
	return false;
}

static const char* GetCurrentModuleDirectory()
{
	static char s_path[MAX_PATH] = { 0 };
	if (s_path[0] == '\0') {
		GetModuleDirectory(s_path, MAX_PATH);
	}
	return s_path;
}

class wdmFileRequestHandler : public Poco::Net::HTTPRequestHandler
{
public:
	wdmFileRequestHandler(const std::string &path)
		: m_path(path)
	{
	}

	void handleRequest(Poco::Net::HTTPServerRequest &request, Poco::Net::HTTPServerResponse &response)
	{
		const char *ext = s_mime_types[0].ext;
		const char *mime = s_mime_types[0].type;
		size_t epos = m_path.find_last_of(".");
		if (epos != std::string::npos) {
			ext = &m_path[epos];
			for (size_t i = 0; i<_countof(s_mime_types); ++i) {
				if (strcmp(ext, s_mime_types[i].ext) == 0) {
					mime = s_mime_types[i].type;
				}
			}
		}
		response.sendFile(m_path, mime);
	}

private:
	std::string m_path;
};

const char s_root_dir[] = "html";

class wdmRequestHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory
{
public:
	virtual Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest &request)
	{
		//			if (wdmSystem::getInstance()->getEndFlag()) { return nullptr; }

		if (request.getURI() == "/") {
			//return new wdmFileRequestHandler(std::string(GetCurrentModuleDirectory()) + std::string(s_root_dir) + "/index.html");
			return new wdmFileRequestHandler(std::string(s_root_dir) + "/index.html");
		}
		//			else if (request.getURI() == "/command" || request.getURI() == "/data") {
		//				return new wdmCommandHandler();
		//			}
		else {
			std::string path = std::string(GetCurrentModuleDirectory()) + std::string(s_root_dir) + request.getURI();
			Poco::File file(path);
			if (file.exists()) {
				return new wdmFileRequestHandler(path);
			}
			else {
				return nullptr;
			}
		}
	}
};

WuiSystem::WuiSystem()
{
	if (!m_server && !m_conf.disabled)
	{
		Poco::Net::HTTPServerParams* params = new Poco::Net::HTTPServerParams;
		params->setMaxQueued(m_conf.max_queue);
		params->setMaxThreads(m_conf.max_threads);
		params->setThreadIdleTime(Poco::Timespan(3, 0));

		try {
			Poco::Net::ServerSocket svs(m_conf.port);
			m_server = new Poco::Net::HTTPServer(new wdmRequestHandlerFactory(), svs, params);
			m_server->start();
		}
		catch (Poco::IOException &) {
		}
	}
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int CmdShow){
	//Load .ini file with all the values
	INIParser* ini = new INIParser(".\\particles.ini");
	ini->loadINI();

	//Initialize window
	WinWindow* wnd = new WinWindow("ParticleSystem");
	wnd->initialize("ParticleSystem", 0, 0, ini->_screenWidth, ini->_screenHeight, NULL, hInstance, ini->_windowed);
	wnd->showWindow(CmdShow);

	//Initialize mouse and keyboard input
	Input* input = new Input();
	if(!input->initialize(hInstance, wnd->getHWND(), ini->_screenWidth, ini->_screenHeight)){
		return -1;
	}

	//Initialize the particle system
	ParticleSystem* ps = new ParticleSystem(input, L"cs.hlsl", ini->_quadLength, ini->_velocityTranslate, ini->_velocityRotate, ini->_maxParticle);
	if(!ps->initialize(hInstance, wnd->getHWND(), ini->_screenWidth, ini->_screenHeight, ini->_initRadius, false, ini->_windowed)){
		return -1;
	}

	//Initialize the timer for fps limiting and time
	Timer fpsTimer;
	Timer timer;
	int frameCounter = 0;
	int fps = 0;
	float maxFPS = static_cast<float>(ini->_maxFramerate);

	fpsTimer.startTimer();
	fpsTimer.getFrameTime();
	timer.startTimer();

	//Delete the ini file though it is not needed anymore
	delete ini;

	MSG msg = {0};
	while(msg.message != WM_QUIT){
		if(PeekMessage(&msg, 0, 0, 0, PM_REMOVE)){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {

			//Only render when since the last renderd image enough time has elapsed
			if(fpsTimer.getFrameTimeWithoutActualisation() > (1.0/maxFPS)){

				ps->setFPSToDraw(fps);
				ps->update(fpsTimer.getFrameTime(), timer.getTime());
				ps->render();

				//Count all the rendered images to display the fps
				frameCounter++;
				if(fpsTimer.getTime() > 1.0f){
					fps = frameCounter;
					frameCounter = 0;
					fpsTimer.startTimer();
				}
			}

		}
	}

	//delete input;
	delete ps;

	return 0;

}

