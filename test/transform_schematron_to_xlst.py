from lxml.isoschematron import Schematron

s = Schematron(file='EPValidator.xml', store_xslt=True)

with open('EPValidator.xslt', 'w') as f:
    f.write(str(s._validator_xslt))
