//// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//// PARTICULAR PURPOSE.
////
//// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once

#include <ppltasks.h>

// A simple reader/writer class that provides support for reading and writing
// files on disk. Provides synchronous and asynchronous methods.
namespace DX
{
	class BasicReaderWriter
	{
	private:
		Windows::Storage::StorageFolder^ m_location;

	public:
		BasicReaderWriter();
		BasicReaderWriter(
			Windows::Storage::StorageFolder^ folder
			);

		Platform::Array<byte>^ ReadData(
			Platform::String^ filename
			);

		concurrency::task<Platform::Array<byte>^> ReadDataAsync(
			Platform::String^ filename
			);

		uint32 WriteData(
			Platform::String^ filename,
			const Platform::Array<byte>^ fileData
			);

		concurrency::task<void> WriteDataAsync(
			Platform::String^ filename,
			const Platform::Array<byte>^ fileData
			);
	};
}

