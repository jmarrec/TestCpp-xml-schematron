#include "XMLValidator.hpp"

#include <fmt/format.h>
#include <libxml/xmlversion.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/schematron.h>

#ifdef USE_LIBXSLT
#  pragma message "USING LIBXLST"
#  include <libxslt/xslt.h>
#  include <libxslt/xsltInternals.h>
#  include <libxslt/transform.h>
#  include <libxslt/xsltutils.h>
#  include <libxml/xpath.h>
#  include <libxml/xpathInternals.h>  // BAD_CAST
#endif

#include <filesystem>
#include <stdexcept>
#include <iostream>

// That's really shitty but it's a global that's needed
extern int xmlLoadExtDtdDefaultValue;

namespace openstudio {

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

std::vector<std::string> XMLValidator::errors() const {
  return m_errors;
}

std::vector<std::string> XMLValidator::warnings() const {
  return {};
}

bool XMLValidator::isValid() const {
  return m_errors.empty();
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

  // xmlSchematronSetValidStructuredErrors(parser_ctxt, (xmlStructuredErrorFunc)fprintf, stderr);
  schema = xmlSchematronParse(parser_ctxt);
  xmlSchematronFreeParserCtxt(parser_ctxt);

  // Start on the document to validate side
  auto filename_str = openstudio::toString(xmlPath);
  const auto* filename = filename_str.c_str();

  /*parse the file and get the DOM */
  doc = xmlReadFile(xmlPath.c_str(), nullptr, 0);

  xmlSchematronValidCtxt* ctxt = nullptr;
  int flag = XML_SCHEMATRON_OUT_TEXT;
  ctxt = xmlSchematronNewValidCtxt(schema, flag);
  if (ctxt == nullptr) {
    // xmlFreeDoc(xmlDoc*);
    throw std::runtime_error("Memory error reading schema in xmlSchematronNewValidCtxt");
  }

  // xmlSchematronSetValidStructuredErrors(ctxt, (xmlStructuredErrorFunc)fprintf, stderr);
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
    xmlChar* error_message = nullptr;
    for (int i = 0; i < nodeset->nodeNr; i++) {
      xmlNode* error_node = nodeset->nodeTab[0];
      error_node = error_node->xmlChildrenNode;
      error_message = xmlNodeListGetString(res, error_node->xmlChildrenNode, 1);
      result.emplace_back((const char*)error_message);
      // while (error_node != nullptr) {
      //   if ((xmlStrcmp(error_node->name, (const xmlChar*)"text") == 0)) {
      //     keyword = xmlNodeListGetString(doc, error_node->xmlChildrenNode, 1);
      //     printf("text: %s\n", keyword);
      //   }

      //   error_node = error_node->next;
      // }
      xmlFree(error_message);
    }
  }

  /* Cleanup of XPath data */
  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);

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
  xsltStylesheet* cur = xsltParseStylesheetFile((const xmlChar*)schematron_filename);

  auto filename_str = openstudio::toString(xmlPath);
  const auto* filename = filename_str.c_str();
  xmlDoc* doc = xmlParseFile(filename);
  xmlDoc* res = xsltApplyStylesheet(cur, doc, params);

  // Dump result of xlstApply
  // xsltSaveResultToFile(stdout, res, cur);

  m_errors = processXSLTApplyResult(res);
  for (const auto& error : m_errors) {
    fmt::print(stderr, "{}\n", error);
  }

  /* dump the resulting document */
  // xmlDocDump(stdout, res);

  xsltFreeStylesheet(cur);
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
  m_errors.clear();
}

}  // namespace openstudio
