#ifndef UTILITIES_CORE_LOGMESSAGE_HPP
#define UTILITIES_CORE_LOGMESSAGE_HPP

#include <string>
#include <vector>

/** Severity levels for logging, Warn is default = 0
 *  Trace = debug messages not currently needed, but may be useful later
 *  Debug = only makes sense to developer trying to track down bug
 *  Info = something a user might want to see if interested (e.g. assumptions made, etc)
 *  Warning = something that probably should not have happened
 *  Error = something that definitely should not have happened
 *  Fatal = something so bad execution can't proceed
 *  Defined at the root namespace level for simplicity */
enum LogLevel
{
  Trace = -3,
  Debug = -2,
  Info = -1,
  Warn = 0,
  Error = 1,
  Fatal = 2
};

namespace openstudio {

/// LogChannel identifies a logger
using LogChannel = std::string;

/// LogMessage encapsulates a single logging message
class LogMessage
{
 public:
  /// constructor
  LogMessage(LogLevel logLevel, std::string logChannel, std::string logMessage);

  /// get the message's log level
  [[nodiscard]] LogLevel logLevel() const;

  /// get the messages log channel
  [[nodiscard]] LogChannel logChannel() const;

  /// get the content of the log message
  [[nodiscard]] std::string logMessage() const;

 private:
  LogLevel m_logLevel;
  std::string m_logChannel;
  std::string m_logMessage;
};

}  // namespace openstudio

#endif  // UTILITIES_CORE_LOGMESSAGE_HPP
