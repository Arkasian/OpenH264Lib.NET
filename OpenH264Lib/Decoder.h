// OpenH264Lib.h

#pragma once

using namespace System;

namespace OpenH264Lib {

	public ref class Decoder
	{
	private:
		ISVCDecoder* decoder;

	private:
		typedef int(__stdcall *WelsCreateDecoderFunc)(ISVCDecoder** ppDecoder);
		WelsCreateDecoderFunc CreateDecoderFunc;
		typedef void(__stdcall *WelsDestroyDecoderFunc)(ISVCDecoder* ppDecoder);
		WelsDestroyDecoderFunc DestroyDecoderFunc;

	private:
		~Decoder(); // �f�X�g���N�^
		!Decoder(); // �t�@�C�i���C�U
	public:
		Decoder(String ^dllName);

	private:
		int Setup();

	public:
		System::Drawing::Bitmap^ Decode(array<Byte> ^frame, int srcLen);
		System::Drawing::Bitmap^ Decode(unsigned char *frame, int srcLen);

	private:
		static byte* YUV420PtoRGBA(byte* yplane, byte* uplane, byte* vplane, int width, int height, int stride);
	};
}
