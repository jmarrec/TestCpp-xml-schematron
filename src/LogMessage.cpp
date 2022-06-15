#include "LogMessage.hpp"

namespace openstudio {

LogMessage::LogMessage(LogLevel logLevel, LogChannel logChannel, std::string logMessage)
  : m_logLevel(logLevel), m_logChannel(std::move(logChannel)), m_logMessage(std::move(logMessage)) {}

LogLevel LogMessage::logLevel() const {
  return m_logLevel;
}

LogChannel LogMessage::logChannel() const {
  return m_logChannel;
}

std::string LogMessage::logMessage() const {
  return m_logMessage;
}

}  // namespace openstudio
