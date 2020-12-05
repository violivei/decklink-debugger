#ifndef __ImageEncoder__
#define __ImageEncoder__

#include "DeckLinkAPI.h"
#include "DeviceProber.h"
#include "util.h"
#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

class ImageEncoder
{
public:
	ImageEncoder(DeviceProber* deviceProber);
	virtual ~ImageEncoder() {}

	virtual void RGBImage(unsigned int);
	virtual bool convertFrameToOpenCV(IDeckLinkVideoFrame* in, cv::Mat &frame);
	virtual std::string EncodeImage();

	virtual ULONG AddRef(void);
	virtual ULONG Release(void);

private:
	IDeckLinkVideoFrame* convertFrameIfReqired(IDeckLinkVideoFrame* frame);
	std::string encodeToPng(IDeckLinkVideoFrame* frame);
	void decodeRawVideoFrame(IDeckLinkVideoFrame* frame, unsigned int iteration);

private:
	int32_t                   m_refCount;
	DeviceProber*             m_deviceProber;
	IDeckLinkVideoConversion* m_frameConverter;
};

#endif
