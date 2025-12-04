#include "XMLValidator.hpp"

#include <fmt/format.h>
#include <libxml/xmlversion.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/schematron.h>

// #  pragma message("USING LIBXLST")
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>  // BAD_CAST

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <stdexcept>

// That's really shitty but it's a global that's needed
extern int xmlLoadExtDtdDefaultValue;

namespace openstudio {

inline const xmlChar* xml_string(const std::string& s) {
  return reinterpret_cast<const xmlChar*>(s.c_str());
}

// Cast size_t to int safely, i.e. throw an exception in case of an overflow.
inline int checked_int_cast(std::size_t len) {
  if (len > INT_MAX) {
    throw std::invalid_argument("data too long");
  }

  return static_cast<int>(len);
}

// Cast int to size_t safely, checking that it's non-negative (we assume that
// size_t is always big enough to contain INT_MAX, which is true for all
// currently existing architectures).
inline std::size_t checked_size_t_cast(int len) {
  if (len < 0) {
    throw std::runtime_error("length value unexpectedly negative");
  }

  return static_cast<std::size_t>(len);
}

// exception safe wrapper around xmlChar*s that are returned from some
// of the libxml functions that the user must free.
class xmlchar_helper
{
 public:
  xmlchar_helper(xmlChar* ptr) : ptr_(ptr) {}

  ~xmlchar_helper() {
    if (ptr_) {
      xmlFree(ptr_);
    }
  }

  const char* get() const {
    return reinterpret_cast<const char*>(ptr_);
  }

 private:
  xmlChar* ptr_;
};

std::string build_message(const std::string& levelName, const xmlError& error) {
  std::ostringstream oss;

  // We currently don't decode domain and code to their symbolic
  // representation as it doesn't seem to be worth it in practice, the error
  // message is usually clear enough, while these numbers can be used for
  // automatic classification of messages.
  oss << "XML " << levelName << " " << error.domain << "." << error.code << ": " << error.message;

  if (error.file) {
    oss << " at " << error.file;
    if (error.line > 0) {
      oss << ":" << error.line;

      // Column information, if available, is passed in the second int
      // field (first one is used with the first string field, see below).
      if (error.int2 > 0) {
        oss << "," << error.int2;
      }
    }
  }

  if (error.str1) {
    oss << " while processing \"" << error.str1 << "\"";
    if (error.int1 > 0) {
      oss << " at position " << error.int1;
    }
  }

  return oss.str();
}

void callback_structured_error(void* userData, xmlError* error) {
  // This shouldn't happen, but better be safe than sorry
  if (!error) {
    return;
  }

  if (error->message) {
    auto* validator = static_cast<XMLValidator*>(userData);

    LogLevel level = LogLevel::Trace;
    std::string levelName;
    if (error->level == XML_ERR_NONE) {
      fmt::print(stderr, "Got a None Error?");
      levelName = "None";
    } else if (error->level == XML_ERR_WARNING) {
      // Some libxml warnings are pretty fatal errors, e.g. failing
      // to open the input file is reported as a warning with code
      // XML_IO_LOAD_ERROR, so treat them as such.
      if (error->code == XML_IO_LOAD_ERROR) {
        level = LogLevel::Error;
        levelName = "error";
      } else {
        level = LogLevel::Warn;
        levelName = "warning";
      }

    } else if (error->level == XML_ERR_ERROR) {
      level = LogLevel::Error;
      levelName = "error";
    } else if (error->level == XML_ERR_FATAL) {
      level = LogLevel::Fatal;
      levelName = "fatal error";
    }

    validator->registerLogMessage(LogMessage(level, "XMLValidator", build_message(levelName, *error)));
  }
}

void XMLValidator::registerLogMessage(LogMessage logMessage) {
  m_logMessages.push_back(std::move(logMessage));
}

// void xmlStructuredErrorFunc(void * userData, xmlErrorPtr error);
//
// struct _xmlError {
//     int domain  : What part of the library raised this error
//     int code  : The error code, e.g. an xmlParserError
//     char *  message : human-readable informative error message
//     xmlErrorLevel level : how consequent is the error
//     char *  file  : the filename
//     int line  : the line number if available
//     char *  str1  : extra string information
//     char *  str2  : extra string information
//     char *  str3  : extra string information
//     int int1  : extra number information
//     int int2  : error column # or 0 if N/A (todo: rename field when we would brk ABI)
//     void *  ctxt  : the parser context if available
//     void *  node  : the node in the tree
// } xmlError;

XMLValidator::XMLValidator(const openstudio::path& xsdPath) : m_xsdPath(std::filesystem::absolute(xsdPath)) {
  if (!openstudio::filesystem::exists(xsdPath)) {
    throw std::runtime_error(openstudio::toString(xsdPath) + "' does not exist");
  } else if (!openstudio::filesystem::is_regular_file(xsdPath)) {
    throw std::runtime_error(openstudio::toString(xsdPath) + "' XSD cannot be opened");
  }
}

XMLValidator::XMLValidator(const std::string& xsdString) : m_xsdString(xsdString) {}

std::optional<openstudio::path> XMLValidator::xsdPath() const {

  return m_xsdPath;
}

std::optional<std::string> XMLValidator::xsdString() const {
  return m_xsdString;
}

std::vector<LogMessage> XMLValidator::errors() const {
  std::vector<LogMessage> result;
  std::copy_if(m_logMessages.cbegin(), m_logMessages.cend(), std::back_inserter(result),
               [](const auto& logMessage) { return logMessage.logLevel() > LogLevel::Warn; });

  return result;
}

std::vector<LogMessage> XMLValidator::warnings() const {
  std::vector<LogMessage> result;
  std::copy_if(m_logMessages.cbegin(), m_logMessages.cend(), std::back_inserter(result),
               [](const auto& logMessage) { return logMessage.logLevel() == LogLevel::Warn; });

  return result;
}

std::string XMLValidator::fullValidationReport() const {
  return m_fullValidationReport;
}

bool XMLValidator::isValid() const {
  return errors().empty();
}

bool XMLValidator::validate(const openstudio::path& xmlPath) {
  if (!openstudio::filesystem::exists(xmlPath)) {
    std::cerr << "Error: '" << toString(xmlPath) << "' does not exist";
    return false;
  } else if (!openstudio::filesystem::is_regular_file(xmlPath)) {
    std::cerr << "Error: '" << toString(xmlPath) << "' XML cannot be opened";
    return false;
  }

  // static char * schematron = NULL;
  // static xmlSchematronPtr wxschematron = NULL;
  auto schematron_filename_str = openstudio::toString(m_xsdPath.value());
  const auto* schematron_filename = schematron_filename_str.c_str();
  xmlSchematron* schema = nullptr;
  xmlDoc* doc = nullptr;

  // That's the context for the schematron part
  xmlSchematronParserCtxt* parser_ctxt = nullptr;
  parser_ctxt = xmlSchematronNewParserCtxt(schematron_filename);
  // or: parser_ctxt = xmlSchematronNewDocParserCtxt(xmlDoc*)
  if (parser_ctxt == nullptr) {
    // xmlFreeDoc(xmlDoc*);
    throw std::runtime_error("Memory error reading schema in xmlSchematronNewParserCtxt");
  }

  schema = xmlSchematronParse(parser_ctxt);
  xmlSchematronFreeParserCtxt(parser_ctxt);

  // Start on the document to validate side
  auto filename_str = openstudio::toString(xmlPath);
  const auto* filename = filename_str.c_str();

  /*parse the file and get the DOM */
  doc = xmlReadFile(filename, nullptr, 0);

  xmlSchematronValidCtxt* ctxt = nullptr;
  int flag = XML_SCHEMATRON_OUT_TEXT;
  ctxt = xmlSchematronNewValidCtxt(schema, flag);
  if (ctxt == nullptr) {
    // xmlFreeDoc(xmlDoc*);
    throw std::runtime_error("Memory error reading schema in xmlSchematronNewValidCtxt");
  }

  xmlSchematronSetValidStructuredErrors(ctxt, callback_structured_error, this);

  // Let's parse the XML document. The parser will cache any grammars encountered.

  int ret = xmlSchematronValidateDoc(ctxt, doc);
  if (ret == 0) {
    fmt::print(stderr, "{} validates\n", filename);
  } else if (ret > 0) {
    fmt::print(stderr, "{} fails to validate\n", filename);
  } else {
    fmt::print(stderr, "{} validation generated an internal error, ret = {}\n", filename, ret);
  }
  xmlSchematronFreeValidCtxt(ctxt);

  xmlSchematronFree(schema);

  xmlFreeDoc(doc);     // free document
  xmlCleanupParser();  // Free globals

  return true;
}

std::vector<std::string> processXSLTApplyResult(xmlDoc* res) {

  xmlXPathContext* xpathCtx = nullptr;
  xmlXPathObject* xpathObj = nullptr;

  const char* xpathExpr = "//svrl:failed-assert";
  /* Create xpath evaluation context */
  xpathCtx = xmlXPathNewContext(res);
  if (xpathCtx == nullptr) {
    throw std::runtime_error("Error: unable to create new XPath context");
  }

  // namesapce
  // xmlns:svrl="http://purl.oclc.org/dsdl/svrl"
  if (xmlXPathRegisterNs(xpathCtx, BAD_CAST "svrl", BAD_CAST "http://purl.oclc.org/dsdl/svrl") != 0) {
    throw std::runtime_error("Error: unable to register NS svrl with prefix");
  }

  /* Evaluate xpath expression */
  xpathObj = xmlXPathEvalExpression((const xmlChar*)xpathExpr, xpathCtx);
  if (xpathObj == nullptr) {
    xmlXPathFreeContext(xpathCtx);
    throw std::runtime_error(fmt::format("Error: unable to evaluate xpath expression '{}'\n", xpathExpr));
  }

  std::vector<std::string> result;

  if (xmlXPathNodeSetIsEmpty(xpathObj->nodesetval)) {
    fmt::print("No errors\n");
    return {};
  } else {

    xmlNodeSet* nodeset = xpathObj->nodesetval;
    for (int i = 0; i < nodeset->nodeNr; i++) {
      xmlNode* error_node = nodeset->nodeTab[i];
      error_node = error_node->xmlChildrenNode;
      xmlchar_helper error_message(xmlNodeListGetString(res, error_node->xmlChildrenNode, 1));

      result.emplace_back(error_message.get());
      // while (error_node != nullptr) {
      //   if ((xmlStrcmp(error_node->name, (const xmlChar*)"text") == 0)) {
      //     keyword = xmlNodeListGetString(doc, error_node->xmlChildrenNode, 1);
      //     printf("text: %s\n", keyword);
      //   }

      //   error_node = error_node->next;
      // }
    }
  }

  /* Cleanup of XPath data */
  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);

  return result;
}

std::string dumpXSLTApplyResultToString(xmlDoc* res, xsltStylesheet* style) {

  xmlChar* xml_string = nullptr;
  int xml_string_length = 0;

  std::string result;

  if (xsltSaveResultToString(&xml_string, &xml_string_length, res, style) == 0) {

    xmlchar_helper helper(xml_string);
    if (xml_string_length > 0) {
      result.assign(helper.get(), checked_size_t_cast(xml_string_length));
    }
  }

  // std::string result((char*)xml_string);
  // xmlFree(xml_string);
  return result;
}

bool XMLValidator::xsltValidate(const openstudio::path& xmlPath) {
  if (!openstudio::filesystem::exists(xmlPath)) {
    std::cerr << "Error: '" << toString(xmlPath) << "' does not exist";
    return false;
  } else if (!openstudio::filesystem::is_regular_file(xmlPath)) {
    std::cerr << "Error: '" << toString(xmlPath) << "' XML cannot be opened";
    return false;
  }

  reset();

  const char* params[16 + 1];
  int nbparams = 0;
  params[nbparams] = nullptr;
  xmlSubstituteEntitiesDefault(1);
  xmlLoadExtDtdDefaultValue = 1;

  auto schematron_filename_str = openstudio::toString(m_xsdPath.value());
  const auto* schematron_filename = schematron_filename_str.c_str();
  xsltStylesheet* style = xsltParseStylesheetFile((const xmlChar*)schematron_filename);

  auto filename_str = openstudio::toString(xmlPath);
  const auto* filename = filename_str.c_str();
  xmlDoc* doc = xmlParseFile(filename);
  xmlDoc* res = xsltApplyStylesheet(style, doc, params);

  // Dump result of xlstApply
  m_fullValidationReport = dumpXSLTApplyResultToString(res, style);
  fmt::print("\n====== Full Validation Report =====\n\n{}", m_fullValidationReport);
  // xsltSaveResultToFile(stdout, res, style);

  auto m_errors = processXSLTApplyResult(res);
  for (const auto& error : m_errors) {
    fmt::print(stderr, "{}\n", error);
    m_logMessages.emplace_back(LogLevel::Error, "processXSLTApplyResult", error);
  }

  /* dump the resulting document */
  // xmlDocDump(stdout, res);

  xsltFreeStylesheet(style);
  xmlFreeDoc(res);
  xmlFreeDoc(doc);

  xsltCleanupGlobals();
  xmlCleanupParser();

  return m_errors.empty();
}

bool XMLValidator::validate(const std::string& /*xmlString*/) {
  return true;
}

void XMLValidator::reset() {
  m_logMessages.clear();
}

}  // namespace openstudio
