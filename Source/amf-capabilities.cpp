/*
MIT License

Copyright (c) 2016-2017

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include "amf-capabilities.h"
#include "misc-util.cpp"

#ifdef _WIN32
#include <windows.h>
#include <VersionHelpers.h>
#endif

using namespace Plugin;
using namespace Plugin::AMD;

#pragma region Singleton
static CapabilityManager* __instance;
static std::mutex __instance_mutex;
void Plugin::AMD::CapabilityManager::Initialize()
{
	const std::lock_guard<std::mutex> lock(__instance_mutex);
	if (!__instance)
		__instance = new CapabilityManager();
}

CapabilityManager* Plugin::AMD::CapabilityManager::Instance() {
	const std::lock_guard<std::mutex> lock(__instance_mutex);
	return __instance;
}

void Plugin::AMD::CapabilityManager::Finalize() {
	const std::lock_guard<std::mutex> lock(__instance_mutex);
	if (__instance)
		delete __instance;
	__instance = nullptr;
}
#pragma endregion Singleton

Plugin::AMD::CapabilityManager::CapabilityManager() {
	// Key order: API, Adapter, Codec
	for (auto api : API::EnumerateAPIs()) {
		for (auto adapter : api->EnumerateAdapters()) {
			for (auto codec : { Codec::H264AVC/*, Codec::H264SVC*/, Codec::HEVC }) {
				AMF_LOG_DEBUG("[Capability Manager] Testing %s Adapter '%s' with codec %s...",
					api->GetName().c_str(), adapter.Name.c_str(), Utility::CodecToString(codec));

				bool isSupported = false;
				try {
					std::unique_ptr<AMD::Encoder> enc;
					if (codec == Codec::H264AVC || codec == Codec::H264SVC) {
						enc = std::make_unique<AMD::EncoderH264>(
							api,
							adapter,
							false, ColorFormat::NV12, ColorSpace::BT709, false);
					} else {
						enc = std::make_unique<AMD::EncoderH265>(
							api, 
							api->GetAdapterById(adapter.idLow, adapter.idHigh),
							false, ColorFormat::NV12, ColorSpace::BT709, false);
					}
					isSupported = true;
				} catch (std::exception& e) {
					AMF_LOG_WARNING("%s", e.what());
				}

				std::tuple<API::Type, API::Adapter, AMD::Codec> key = std::make_tuple(api->GetType(), adapter, codec);
				m_CapabilityMap.insert_or_assign(key, isSupported);
			}
		}
	}
}

Plugin::AMD::CapabilityManager::~CapabilityManager() {}

bool Plugin::AMD::CapabilityManager::IsCodecSupported(AMD::Codec codec) {
	for (auto api : API::EnumerateAPIs()) {
		if (IsCodecSupportedByAPI(codec, api->GetType()))
			return true;
	}
	return false;
}

bool Plugin::AMD::CapabilityManager::IsCodecSupportedByAPI(AMD::Codec codec, API::Type type) {
	auto api = API::GetAPI(type);
	for (auto adapter : api->EnumerateAdapters()) {
		if (IsCodecSupportedByAPIAdapter(codec, type, adapter) == true)
			return true;
	}
	return false;
}

bool Plugin::AMD::CapabilityManager::IsCodecSupportedByAPIAdapter(AMD::Codec codec, API::Type api, API::Adapter adapter) {
	return m_CapabilityMap[std::make_tuple(api, adapter, codec)];
}
