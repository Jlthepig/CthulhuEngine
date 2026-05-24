// log_utils.hpp
//
// This is free source code, and you are welcome to redistribute it under certain conditions.
// Read LICENSE.md for more information.
//
// Provides:
//   - detailed logger - time, date, log type, origin tag
//   - simple logger - just a fwrite to the console with a single string parameter
//   - log types - info (no log type stamp), debug (skipped in release), success, warning, error
//   - time stamp, date stamp accurate to system clock
//   - logHook - user-defined function that allows emitting logs to another target like the crash log storage in kalawindow
//

#pragma once

#include <cstring>
#include <ctime>
#include <cstdio>
#include <string>
#include <chrono>
#include <array>
#include <algorithm>


//static_cast
#ifndef scast
	#define scast static_cast
#endif

//
// CROSS-PLATFORM DEBUG FLAG
//

#ifndef KDEBUG
	#if defined(_MSC_VER) && defined(_DEBUG)
		#define KDEBUG
	#elif (defined(__GNUC__) || defined(__clang__)) && !defined(NDEBUG)
		#define KDEBUG
	#endif
#endif

namespace KalaHeaders::KalaLog
{
	using std::string;
	using std::string_view;
	using std::chrono::system_clock;
	using std::chrono::duration_cast;
	using std::chrono::microseconds;
	using std::array;
	using std::fwrite;
	using std::fflush;
	using std::clamp;
	using std::memset;
	using std::memcpy;
	using std::strftime;
	using std::snprintf;
	
	using u8 = uint8_t;
	using u16 = uint16_t;
	using u32 = uint32_t;

	//Max allowed print message length
	constexpr u16 MAX_MESSAGE_LENGTH = 5000;
	//Max allowed full print tag length
	constexpr u8 MAX_TAG_LENGTH = 50;
	//Max allowed indentation length per message
	constexpr u8 MAX_INDENT_LENGTH = 20;
	//How many type + tag combinations are cached
	constexpr u8 CACHED_TAGS_LENGTH = 50;

	enum class LogType
	{
		LOG_VERBOSE, //Spammy, frequent log message for detailed logs, sent to stdout
		LOG_INFO,    //General-purpose log message, sent to stdout
		LOG_DEBUG,   //Debugging message, only appears in debug builds, sent to stdout
		LOG_SUCCESS, //Confirmation that an operation succeeded, sent to stdout
		LOG_WARNING, //Non-critical issue that should be looked into, sent to stdout
		LOG_ERROR    //Serious issue or failure, sent to stderr, always flushes
	};
	enum class TimeFormat : u8
	{
		TIME_NONE           = 0, //No time stamp
		TIME_DEFAULT        = 1, //Uses TIME_HMS_MS_US
		TIME_HMS            = 2, //23:59:59
		TIME_HMS_MS         = 3, //23:59:59:123
		TIME_HMS_MS_US      = 4, //23:59:59:123:456
		TIME_12H            = 5, //11:59:59 PM
		TIME_ISO_8601       = 6, //23:59:59Z
		TIME_FILENAME       = 7, //23-59-59
		TIME_FILENAME_MS    = 8, //23-59-59-123
		TIME_FILENAME_MS_US = 9  //23-59-59-123-456
	};
	enum class DateFormat : u8
	{
		DATE_NONE         = 0, //No date stamp
		DATE_DEFAULT      = 1, //Uses DATE_NONE
		DATE_DMY          = 2, //31/12/2026
		DATE_MDY          = 3, //12/31/2026
		DATE_ISO_8601     = 4, //2026-12-31
		DATE_TEXT_DMY     = 5, //31 December, 2026
		DATE_TEXT_MDY     = 6, //December 31, 2026
		DATE_FILENAME_DMY = 7, //31-12-2026
		DATE_FILENAME_MDY = 8  //12-31-2026
	};

	struct CachedPrefix
	{
		LogType type{};
		string target{};
		string prefix{};
	};

	class Log
	{
	public:
		//Returns current time in chosen or default format
		static inline string_view GetTime(TimeFormat timeFormat = TimeFormat::TIME_DEFAULT)
		{
			static thread_local const string empty{};

			//return empty for OOB or none
			if (timeFormat == TimeFormat::TIME_NONE
				|| timeFormat > TimeFormat::TIME_FILENAME_MS_US)
			{
				return empty;
			}

			if (timeFormat == TimeFormat::TIME_DEFAULT)
			{
				return GetTime(TimeFormat::TIME_HMS_MS_US);
			}

			static thread_local tm cachedLocal{};
			static thread_local tm cachedUTC{};

			const auto now = system_clock::now();
			const auto us_since_epoch = duration_cast<microseconds>(now.time_since_epoch()).count();

			const auto in_time_t = system_clock::to_time_t(now);
			const int us = us_since_epoch % 1000;          //true microsecond precision
			const int ms = (us_since_epoch / 1000) % 1000; //sub-millisecond precision

#ifdef _WIN32
			localtime_s(&cachedLocal, &in_time_t);
			gmtime_s(&cachedUTC, &in_time_t);
#else
			localtime_r(&in_time_t, &cachedLocal);
			gmtime_r(&in_time_t, &cachedUTC);
#endif

			static thread_local char buffer[20]{};
			switch (timeFormat)
			{
			case TimeFormat::TIME_HMS:
			{
				strftime(buffer, sizeof(buffer), "%H:%M:%S", &cachedLocal);

				break;
			}
			case TimeFormat::TIME_HMS_MS:
			{
				char tmp[20]{};
				size_t length = strftime(tmp, sizeof(tmp), "%H:%M:%S", &cachedLocal);

				memcpy(buffer, tmp, length);
				buffer[length++] = ':';
				buffer[length++] = '0' + (ms / 100) % 10;
				buffer[length++] = '0' + (ms / 10) % 10;
				buffer[length++] = '0' + (ms % 10);
				buffer[length] = '\0';

				break;
			}
			case TimeFormat::TIME_HMS_MS_US:
			{
				char tmp[20]{};
				size_t length = strftime(tmp, sizeof(tmp), "%H:%M:%S", &cachedLocal);

				memcpy(buffer, tmp, length);
				buffer[length++] = ':';
				buffer[length++] = '0' + (ms / 100) % 10;
				buffer[length++] = '0' + (ms / 10) % 10;
				buffer[length++] = '0' + (ms % 10);
				buffer[length++] = ':';
				buffer[length++] = '0' + (us / 100) % 10;
				buffer[length++] = '0' + (us / 10) % 10;
				buffer[length++] = '0' + (us % 10);
				buffer[length] = '\0';

				break;
			}
			case TimeFormat::TIME_12H:
			{
				strftime(buffer, sizeof(buffer), "%I:%M:%S %p", &cachedLocal);

				break;
			}
			case TimeFormat::TIME_ISO_8601:
			{
				strftime(buffer, sizeof(buffer), "%H:%M:%SZ", &cachedUTC);

				break;
			}
			case TimeFormat::TIME_FILENAME:
			{
				strftime(buffer, sizeof(buffer), "%H-%M-%S", &cachedLocal);

				break;
			}
			case TimeFormat::TIME_FILENAME_MS:
			{
				char tmp[20]{};
				size_t length = strftime(tmp, sizeof(tmp), "%H-%M-%S", &cachedLocal);

				memcpy(buffer, tmp, length);
				buffer[length++] = '-';
				buffer[length++] = '0' + (ms / 100) % 10;
				buffer[length++] = '0' + (ms / 10) % 10;
				buffer[length++] = '0' + (ms % 10);
				buffer[length] = '\0';

				break;
			}
			case TimeFormat::TIME_FILENAME_MS_US:
			{
				char tmp[20]{};
				size_t length = strftime(tmp, sizeof(tmp), "%H-%M-%S", &cachedLocal);

				memcpy(buffer, tmp, length);
				buffer[length++] = '-';
				buffer[length++] = '0' + (ms / 100) % 10;
				buffer[length++] = '0' + (ms / 10) % 10;
				buffer[length++] = '0' + (ms % 10);
				buffer[length++] = '-';
				buffer[length++] = '0' + (us / 100) % 10;
				buffer[length++] = '0' + (us / 10) % 10;
				buffer[length++] = '0' + (us % 100);
				buffer[length] = '\0';

				break;
			}
			default: return empty;
			}

			return string_view(buffer);
		}
		//Returns current date in chosen or default format
		static inline string_view GetDate(DateFormat dateFormat = DateFormat::DATE_DEFAULT)
		{
			static thread_local string empty{};

			//return empty for OOB or default or none
			if (dateFormat < DateFormat::DATE_DMY
				|| dateFormat > DateFormat::DATE_FILENAME_MDY)
			{
				return empty;
			}

			static thread_local string cached[scast<int>(DateFormat::DATE_FILENAME_MDY) + 1];
			static thread_local int last_yday = -1;
			static thread_local tm cachedLocal{};

			const int idx = scast<int>(dateFormat);
			const auto now = system_clock::now();

			const auto in_time_t = system_clock::to_time_t(now);

#ifdef _WIN32
			localtime_s(&cachedLocal, &in_time_t);
#else
			localtime_r(&in_time_t, &cachedLocal);
#endif

			if (!cached[idx].empty()
				&& cachedLocal.tm_yday == last_yday)
			{
				return cached[idx];
			}

			last_yday = cachedLocal.tm_yday;

			char buffer[64]{};
			switch (dateFormat)
			{
			case DateFormat::DATE_DMY:          strftime(buffer, sizeof(buffer), "%d/%m/%Y", &cachedLocal); break;
			case DateFormat::DATE_MDY:          strftime(buffer, sizeof(buffer), "%m/%d/%Y", &cachedLocal); break;
			case DateFormat::DATE_ISO_8601:     strftime(buffer, sizeof(buffer), "%Y-%m-%d", &cachedLocal); break;
			case DateFormat::DATE_TEXT_DMY:     strftime(buffer, sizeof(buffer), "%d %B, %Y", &cachedLocal); break;
			case DateFormat::DATE_TEXT_MDY:     strftime(buffer, sizeof(buffer), "%B %d, %Y", &cachedLocal); break;
			case DateFormat::DATE_FILENAME_DMY: strftime(buffer, sizeof(buffer), "%d-%m-%Y", &cachedLocal); break;
			case DateFormat::DATE_FILENAME_MDY: strftime(buffer, sizeof(buffer), "%m-%d-%Y", &cachedLocal); break;
			default:                            return empty;
			}

			cached[idx] = buffer;
			return cached[idx];
		}

		//Prints a log message to the console using fwrite.
		//A newline is added automatically so std::endline or \n is not needed.
		//  - message: the actual message of this log, clamped up to 2000 characters
		//  - target: name of the namespace, class, function or variable of this log, clamped up to 50 characters
		//  - type: sets the tag type, LOG_INFO has no tag, error always flushes
		//  - indentation: optional leading space count in after time and date stamp, clamped up to 20
		//  - flush: set to true for crash logs, diagnostics, assertion failures
		//  - timeFormat: optional time stamp
		//  - dateFormat: optional date stamp
		static inline void Print(
			string_view message,
			string_view target,
			LogType type,
			unsigned int indentation = 0,
			bool flush = false,
			TimeFormat timeFormat = TimeFormat::TIME_DEFAULT,
			DateFormat dateFormat = DateFormat::DATE_DEFAULT)
		{
#ifndef KDEBUG
			if (type == LogType::LOG_DEBUG) return;
#endif

			thread_local const string empty{};

			if (message.empty()
				|| target.empty())
			{
				return;
			}

			string trimmed = TrimUTF8(message);

			target = target.substr(0, MAX_TAG_LENGTH);

			string_view timeStamp = GetTime(timeFormat);
			string_view dateStamp = GetDate(dateFormat);

			string_view prefix = GetCachedPrefix(type, target);

			char* p = logBuffer().data();

			FILE* out = (type == LogType::LOG_ERROR)
				? stderr
				: stdout;

			//color open — LOG_INFO has an empty code (length 0) so memcpy is a no-op
			const size_t colorLen = LogTypeColorLength[scast<size_t>(type)];
			memcpy(p, LogTypeColor[scast<size_t>(type)], colorLen);
			p += colorLen;

			//append [ date ] [ time ]
			if (!dateStamp.empty())
			{
				*p++ = '[';
				*p++ = ' ';
				memcpy(p, dateStamp.data(), dateStamp.size());
				p += dateStamp.size();
				*p++ = ' ';
				*p++ = ']';
				*p++ = ' ';
			}
			if (!timeStamp.empty())
			{
				*p++ = '[';
				*p++ = ' ';
				memcpy(p, timeStamp.data(), timeStamp.size());
				p += timeStamp.size();
				*p++ = ' ';
				*p++ = ']';
				*p++ = ' ';
			}

			//indentation
			if (indentation > 0)
			{
				unsigned int clamped = clamp(
					indentation,
					0u,
					scast<unsigned int>(MAX_INDENT_LENGTH));
					
				memset(p, ' ', clamped);
				p += clamped;
			}

			//cached prefix
			memcpy(p, prefix.data(), prefix.size());
			p += prefix.size();

			//trimmed message
			memcpy(p, trimmed.data(), trimmed.size());
			p += trimmed.size();

			//color reset — skipped for LOG_INFO since we never opened a code
			if (colorLen > 0)
			{
				memcpy(p, ANSI_RESET, ANSI_RESET_LEN);
				p += ANSI_RESET_LEN;
			}

			//newline
			*p++ = '\n';

			const size_t length = scast<size_t>(p - logBuffer().data());

			fwrite(logBuffer().data(), 1, length, out);

			if (flush
				|| type == LogType::LOG_ERROR)
			{
				fflush(out);
			}
		}

		//Prints a log message to the console using fwrite.
		//A newline is added automatically so std::endline or \n is not needed.
		//  - message: the actual message of this log
		//  - flush: set to true for crash logs, diagnostics, assertion failures
		static inline void Print(
			string_view message,
			bool flush = false)
		{
			if (message.empty()) return;

			string trimmed = TrimUTF8(message);

			const size_t length = trimmed.size();
			const size_t totalLength = length + 1; //+1 for '\n'

			memcpy(logBuffer().data(), trimmed.data(), length);
			logBuffer()[length] = '\n';

			fwrite(logBuffer().data(), 1, totalLength, stdout);

			if (flush) fflush(stdout);
		}

	private:
		//---------------------------------------------------------------------
		// ANSI color codes
		//   VERBOSE -> dark grey   \x1b[90m
		//   INFO    -> default     (no code, empty)
		//   DEBUG   -> cyan        \x1b[36m
		//   SUCCESS -> green       \x1b[32m
		//   WARNING -> yellow      \x1b[33m
		//   ERROR   -> bright red  \x1b[91m
		//---------------------------------------------------------------------
		static constexpr const char* ANSI_RESET    = "\x1b[0m";
		static constexpr size_t      ANSI_RESET_LEN = 4;

		static constexpr const char* LogTypeColor[] =
		{
			"\x1b[90m", //LOG_VERBOSE  dark grey
			"",          //LOG_INFO     default (no code)
			"\x1b[36m", //LOG_DEBUG    cyan
			"\x1b[32m", //LOG_SUCCESS  green
			"\x1b[33m", //LOG_WARNING  yellow
			"\x1b[91m"  //LOG_ERROR    bright red
		};
		static constexpr size_t LogTypeColorLength[] =
		{
			7, //"\x1b[90m"
			0, //""
			7, //"\x1b[36m"
			7, //"\x1b[32m"
			7, //"\x1b[33m"
			7  //"\x1b[91m"
		};

		static inline string TrimUTF8(string_view s)
		{
			size_t bytes = 0;
			size_t chars = 0;
			
			while (bytes < s.size() 
				&& chars < MAX_MESSAGE_LENGTH)
			{
				unsigned char c = scast<unsigned char>(s[bytes]);
				
				size_t charLen = 
					(c < 0x80) ? 1 
					: (c < 0xE0) ? 2 
					: (c < 0xF0) ? 3 
					: 4;
					
				if (bytes + charLen > s.size()) break; //incomplete char
				
				bytes += charLen;
				++chars;
			}
			
			string result(s.data(), bytes);
			
			if (bytes < s.size()) result.append("\n[TRIMMED LONG MESSAGE]");
			
			return result;
		};

		static constexpr const char* LogTypeTag[] =
		{
			"VERBOSE | ",
			"",           //LOG_INFO
			"DEBUG | ",
			"SUCCESS | ",
			"WARNING | ",
			"ERROR | "
		};
		static constexpr array<size_t, 6> LogTypeTagLength =
		{
			10,
			0, 
			8, 
			10, 
			10, 
			8
		};

		static inline string_view GetCachedPrefix(
			LogType type,
			string_view target)
		{
			//search existing entries

			for (size_t i = 0; i < prefixSize(); ++i)
			{
				const auto& e = prefixCache()[i];
				if (e.type == type
					&& e.target == target)
				{
					return e.prefix;
				}
			}

			//not found, make new

			const char* tag = LogTypeTag[scast<size_t>(type)];
			const size_t tagLength = LogTypeTagLength[scast<size_t>(type)];
			const size_t targetLength = target.size();

			//"[ " + tag + " | " + target + "] " 

			string built{};
			built.resize(2 + tagLength + targetLength + 3);

			char* p = built.data();
			p[0] = '[';
			p[1] = ' ';
			memcpy(p + 2, tag, tagLength);
			memcpy(p + 2 + tagLength, target.data(), targetLength);
			p[2 + tagLength + targetLength] = ' '; 
			p[3 + tagLength + targetLength] = ']';
			p[4 + tagLength + targetLength] = ' ';

			size_t& size  = prefixSize();
			size_t& clock = prefixClock();

			size_t index{};
			if (size < prefixCache().size()) index = size++;
			else index = (clock++ % prefixCache().size());

			prefixCache()[index] = { type, string(target), std::move(built) };
			return prefixCache()[index].prefix;
		}

		//Message length + headroom for tag, date stamp, time stamp, indent, and color codes
		//Worst case: MAX_MESSAGE_LENGTH + time(20) + date(12) + MAX_INDENT_LENGTH(20)
		//          + MAX_TAG_LENGTH(50) + brackets/spaces(~20) + color open(7) + reset(4) + newline(1)
		static inline array<char, MAX_MESSAGE_LENGTH + 256>& logBuffer()
		{
			thread_local array<char, MAX_MESSAGE_LENGTH + 256> buffer{};
			return buffer;
		}

		static inline array<CachedPrefix, CACHED_TAGS_LENGTH>& prefixCache()
		{
			thread_local array<CachedPrefix, CACHED_TAGS_LENGTH> cache{};
			return cache;
		}

		//Moved into accessor functions to avoid the thread_local static inline member pitfall
		static inline size_t& prefixSize()
		{
			thread_local size_t val{};
			return val;
		}
		static inline size_t& prefixClock()
		{
			thread_local size_t val{};
			return val;
		}
	};
}