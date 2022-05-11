#include <gtest/gtest.h>

#include <cstdio>
#include <libxml/xmlversion.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <fmt/format.h>

#include "../src/XMLValidator.hpp"
#include "../src/Filesystem.hpp"

#include <src/resources.hxx>

static void print_element_names(xmlNode* a_node) {
  xmlNode* cur_node = nullptr;
  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      printf("node type: Element, name: %s\n", cur_node->name);
    }
    print_element_names(cur_node->children);
  }
}
TEST(LibXMLTest, basic) {
  xmlDoc* doc = nullptr;
  xmlNode* root_element = nullptr;

  std::string xmlPath(openstudio::toString(testDirPath() / "books.xml"));

  LIBXML_TEST_VERSION

  /*parse the file and get the DOM */
  doc = xmlReadFile(xmlPath.c_str(), nullptr, 0);

  ASSERT_NE(nullptr, doc) << "error: could not parse file " << xmlPath;

  /*Get the root element node */
  root_element = xmlDocGetRootElement(doc);
  print_element_names(root_element);
  xmlFreeDoc(doc);     // free document
  xmlCleanupParser();  // Free globals
}

TEST(LibXMLTest, XMLValidator) {
  openstudio::filesystem::path xmlPath = testDirPath() / "books.xml";
  openstudio::filesystem::path schematronPath = testDirPath() / "books.sct";

  openstudio::XMLValidator xmlValidator(schematronPath);
  xmlValidator.validate(xmlPath);
}

TEST(LibXMLTest, XMLValidator_HPXMLvalidator) {
  // I had to modify both the XML document and the Schematron XML so it'd work with the old schematron implementation of libxml
  openstudio::filesystem::path xmlPath = testDirPath() / "base_mod.xml";
  openstudio::filesystem::path schematronPath = testDirPath() / "HPXMLvalidator.sct";

  openstudio::XMLValidator xmlValidator(schematronPath);
  xmlValidator.validate(xmlPath);
}

TEST(LibXMLTest, XMLValidator_EPvalidator) {
  openstudio::filesystem::path xmlPath = testDirPath() / "base_mod.xml";
  openstudio::filesystem::path schematronPath = testDirPath() / "EPvalidator.sct";

  openstudio::XMLValidator xmlValidator(schematronPath);
  xmlValidator.validate(xmlPath);
}

TEST(LibXMLTest, XMLValidator_HPXMLvalidator_XSLT) {
  openstudio::filesystem::path xmlPath = testDirPath() / "base.xml";
  openstudio::filesystem::path schematronPath = testDirPath() / "HPXMLvalidator.xslt";

  openstudio::XMLValidator xmlValidator(schematronPath);
  EXPECT_FALSE(xmlValidator.xsltValidate(xmlPath));

  auto errors = xmlValidator.errors();
  EXPECT_EQ(1, errors.size());
  EXPECT_EQ("Expected EventType to be 'audit' or 'proposed workscope' or 'approved workscope' or 'construction-period testing/daily test out' or "
            "'job completion testing/final inspection' or 'quality assurance/monitoring' or 'preconstruction'",
            errors[0]);
}
