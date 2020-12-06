#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <chrono>
#include <thread>
#include <time.h>
#include <atomic>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

#include "util.h"
#include "tostring.h"
#include "DeckLinkAPI.h"
#include "DeviceProber.h"
#include "TablePrinter.h"

#include "RefReleaser.hpp"
#include "scope_guard.hpp"
#include "log.h"
#include "MutableVideoFrame.h"
#include "util.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <opencv2/opencv.hpp>

static std::atomic<bool> g_do_exit{false};
std::chrono::system_clock::time_point a = std::chrono::system_clock::now();
std::chrono::system_clock::time_point b = std::chrono::system_clock::now();
std::vector<DeviceProber*> createDeviceProbers();
void freeDeviceProbers(std::vector<DeviceProber*> deviceProbers);

void printStatusList(std::vector<DeviceProber*> deviceProbers, unsigned int iteration);
char* getDeviceName(IDeckLink* deckLink);

static void sigfunc(int signum);


bool convertFrameToOpenCV(IDeckLinkVideoFrame* in, cv::Mat &frame)
{
	switch (in->GetPixelFormat()) {
    case bmdFormat8BitYUV:
    {
        void* data;
        if (FAILED(in->GetBytes(&data)))
            return false;

        cv::Mat mat = cv::Mat(in->GetHeight(), in->GetWidth(), CV_8UC2, data,
            in->GetRowBytes());

		// std::lock_guard<std::mutex> g( mtxFrameBytes);
        cv::cvtColor(mat, frame, cv::COLOR_YUV2BGR_UYVY);
        return true;
    }
    case bmdFormat8BitBGRA:
    {
        void* data;
        if (FAILED(in->GetBytes(&data)))
            return false;

		cv::Mat mat = cv::Mat(in->GetHeight(), in->GetWidth(), CV_8UC4, data);

		cv::cvtColor(mat, frame, cv::COLOR_BGRA2BGR);
        return true;
    }
    default:
    {
		std::cout << "Other pixel format" << std::endl;
		return false;
    }}
}

void decodeRawVideoFrame(IDeckLinkVideoFrame* videoFrame, unsigned int iteration)
{
	cv::Mat mFrame;
    if(videoFrame)
    {
        if (videoFrame->GetFlags() & bmdFrameHasNoInputSource)
        {
			std::cerr << "Frame received - No input signal detected\n" << std::endl;
        }
        else
        {
			convertFrameToOpenCV( videoFrame, mFrame );
			if (!mFrame.empty())
			{
				std::stringstream ss;
				int n_digits = 3;
				std::string prefix = "file";
				std::string ext(".png");
				ss << prefix << std::setfill('0') << std::setw(n_digits) << iteration << ext;
				cv::imwrite(ss.str(), mFrame);
			}
        }
		videoFrame->Release();
	}
}

IDeckLinkVideoFrame* convertFrameIfReqired(IDeckLinkVideoFrame* frame, IDeckLinkVideoConversion* m_frameConverter)
{
	HRESULT result;

	if(frame->GetPixelFormat() == bmdFormat8BitBGRA)
	{
		return frame;
	}

	IDeckLinkVideoFrame* convertedFrame = new MutableVideoFrame(
		frame->GetWidth(),
		frame->GetHeight(),
		bmdFormat8BitBGRA);

	result = m_frameConverter->ConvertFrame(frame, convertedFrame);
	if (result != S_OK)
	{
		fprintf(stderr, "Failed to convert frame\n");
		exit(1);
	}

	frame->Release();
	return convertedFrame;
}

void _main() {
	LOG(DEBUG) << "creating Device-Probers";
	std::vector<DeviceProber*> deviceProbers = createDeviceProbers();

	auto deviceProbersGuard = sg::make_scope_guard([deviceProbers]{
		LOG(DEBUG) << "freeing Device-Probers";
		freeDeviceProbers(deviceProbers);

		std::cout << "Bye!" << std::endl;
	});

	if(deviceProbers.size() == 0)
	{
		std::cerr << "Decklink-Driver is available but no DeckLink devices were found." << std::endl <<
			"Check `BlackmagicFirmwareUpdater status`, `dmesg` and `lspci`." << std::endl << std::endl;
		throw "No DeckLink devices found";
	}

	LOG(DEBUG2) << "registering Signal-Handler";
	signal(SIGINT, sigfunc);
	signal(SIGTERM, sigfunc);
	signal(SIGHUP, sigfunc);

	LOG(DEBUG2) << "entering Display-Loop";
	std::ostringstream oss;
	std::vector<IDeckLinkVideoInputFrame*> m_deckLinkVideoFrames;
	std::ofstream outFile;

	unsigned int iteration = 0;
	std::chrono::duration<double, std::milli> work_time;
	std::chrono::duration<double, std::milli> sleep_time;
	bool connected = false;
	IDeckLinkVideoConversion* m_frameConverter = CreateVideoConversionInstance();

	while(!connected)
	{	
		// Waiting for connection with device prober
		for(DeviceProber* deviceProber: deviceProbers) {
			if(!deviceProber->GetSignalDetected()) {
				deviceProber->SelectNextConnection();
			}
			connected = true;
		}
	}

	// Waiting for playback to finish
	clock_t start = clock();
	while ((clock() - start)/CLOCKS_PER_SEC <= 1)
	{
		// printf("Time: %f \n", (clock() - start)/CLOCKS_PER_SEC);
	}

	// Grab image frames from VideoInputFrameArrived callback
	for(DeviceProber* deviceProber: deviceProbers) {
		m_deckLinkVideoFrames = deviceProber->GetFrames();
	}

	iteration = 0;
	printf("Write Images\n");
	for(IDeckLinkVideoFrame* m_frame : m_deckLinkVideoFrames)
	{	
		printf("Imagem: %d \n", iteration);
		m_frame = convertFrameIfReqired(m_frame, m_frameConverter);
		decodeRawVideoFrame(m_frame, iteration);
		iteration++;
	}

	std::cout << "Cleaning up…" << std::endl;
}

int main (UNUSED int argc, UNUSED char** argv)
{
	char c;
	while ((c = getopt(argc, argv, "v")) != -1) {
		switch (c) {
			case 'v':
				Log::IncrementReportingLevel();
				break;
		}
	}

	if(Log::ReportingLevel() > ERROR) {
		std::cerr << "Log-Level set to " << Log::ToString(Log::ReportingLevel()) << std::endl << std::endl;
	}

	try {
		_main();
	}
	catch(const char* e) {
		std::cerr << "exception cought: " << e << std::endl;
		return 1;
	}
	catch(...) {
		std::cerr << "unknown exception cought" << std::endl;
		return 1;
	}

	return 0;
}

static void sigfunc(int signum)
{
	LOG(INFO) << "cought signal "<< signum;
	if (signum == SIGINT || signum == SIGTERM)
	{
		LOG(DEBUG) << "g_do_exit = true";
		g_do_exit = true;
	}
}

std::vector<DeviceProber*> createDeviceProbers()
{
	IDeckLinkIterator*    deckLinkIterator = CreateDeckLinkIteratorInstance();
	RefReleaser<IDeckLinkIterator> deckLinkIteratorReleaser(&deckLinkIterator);
	if(deckLinkIterator == nullptr) {
		std::cerr << "The DeckLink Device-Iterator could not be created. " << std::endl <<
			"Check if the DeckLink Kernel-Module is correctly installed: `lsmod | grep blackmagic`." << std::endl << std::endl;
		throw "error creating IDeckLinkIterator";
	}

	std::vector<DeviceProber*> deviceProbers;

	unsigned int i = 0;
	IDeckLink* deckLink = nullptr;
	try {
		while (deckLinkIterator->Next(&deckLink) == S_OK)
		{
			RefReleaser<IDeckLink> deckLinkReleaser(&deckLink);
			i++;
			LOG(DEBUG1) << "creating DeviceProber for Device " << i;
			deviceProbers.push_back(new DeviceProber(deckLink));
			LOG(INFO) << "-----------------------------";
		}
	}
	catch(...) {
		LOG(ERROR) << "cought exception, freeing already created DeviceProbers";
		LOG(INFO) << "-----------------------------";
		freeDeviceProbers(deviceProbers);
		throw;
	}

	return deviceProbers;
}

void freeDeviceProbers(std::vector<DeviceProber*> deviceProbers)
{
	unsigned int i = 0;
	for(DeviceProber* deviceProber : deviceProbers)
	{
		i++;
		LOG(DEBUG1) << "freeing DeviceProber for Device " << i;
		delete deviceProber;
		LOG(INFO) << "-----------------------------";
	}
	deviceProbers.clear();
}

void printStatusList(std::vector<DeviceProber*> deviceProbers, unsigned int iteration)
{
	if(iteration > 0)
	{
		int nLines = deviceProbers.size() + 6;
		std::cout << "\033[" << nLines << "A";
	}

	bprinter::TablePrinter table(&std::cout);
	table.AddColumn("#", 15);
	table.AddColumn("Device Name", 31);
	table.AddColumn("Can Input & Detect", 20);
	table.AddColumn("Signal Detected", 17);
	table.AddColumn("Active Connection", 19);
	table.AddColumn("Detected Mode", 16);
	table.AddColumn("Pixel Format", 15);
	// table.AddColumn("Iteration", 1);
	table.set_flush_left();
	table.PrintHeader();

	int deviceIndex = 0;
	for(DeviceProber* deviceProber : deviceProbers)
	{
		if(!deviceProber->GetSignalDetected())
		{
			table << bprinter::greyon();
		}

		std::string deviceName = deviceProber->GetDeviceName();
		if(deviceProber->IsSubDevice())
		{
			deviceName = "\\-> " + deviceName;
		}

		table
			<< deviceIndex
			<< deviceName
			<< boolToString(deviceProber->CanAutodetect() && deviceProber->CanInput())
			<< boolToString(deviceProber->GetSignalDetected())
			<< videoConnectionToString(deviceProber->GetActiveConnection())
			<< deviceProber->GetDetectedMode()
			<< pixelFormatToString(deviceProber->GetPixelFormat())
			<< bprinter::greyoff();

		deviceIndex++;
	}
	table.PrintFooter();

	const char iterationSign[4] = { '|', '\\', '-', '/' };
	std::cout << std::endl << "     Scanning... " << iterationSign[iteration % 4] << std::endl;
}
