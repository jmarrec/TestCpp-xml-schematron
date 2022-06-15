#ifndef XMLVALIDATOR_HPP
#define XMLVALIDATOR_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Filesystem.hpp"
#include "LogMessage.hpp"

typedef struct _xmlError xmlError;

namespace openstudio {
class XMLValidator
{
 public:
  /** @name Constructors */
  //@{

  /// Constructor for a new validator
  explicit XMLValidator(const openstudio::path& xsdPath);

  explicit XMLValidator(const std::string& xsdString);

  XMLValidator(XMLValidator const& other) = delete;
  XMLValidator& operator=(XMLValidator const& other) = delete;

  XMLValidator(XMLValidator&& other) = default;
  XMLValidator& operator=(XMLValidator&& other) = default;

  //@}
  /** @name Destructor */
  //@{

  // ~XMLValidator();  // Only reason I declare this (and don't define, is because I am using a std::unique_ptr to classes with incomplete types

  //@}
  /** @name Getters */
  //@{

  std::optional<openstudio::path> xsdPath() const;

  std::optional<std::string> xsdString() const;

  std::vector<LogMessage> errors() const;

  std::vector<LogMessage> warnings() const;

  bool isValid() const;

  std::string fullValidationReport() const;

  //@}
  /** @name Setters */
  //@{

  bool validate(const openstudio::path& xmlPath);

  bool validate(const std::string& xmlString);

  bool xsltValidate(const openstudio::path& xmlPath);

  //@}
  /** @name callbacks */
  //@{

  //@}

 protected:
  void setParser();
  friend void callback_structured_error(void* userData, xmlError* error);
  void registerLogMessage(LogMessage logMessage);

 private:
  // REGISTER_LOGGER("openstudio.XMLValidator");

  void reset();

  std::optional<openstudio::path> m_xsdPath;  // TODO: replace to path
  std::optional<std::string> m_xsdString;

  std::vector<LogMessage> m_logMessages;

  std::string m_fullValidationReport;
};
}  // namespace openstudio
#endif /* ifndef XMLVALIDATOR_HPP */
