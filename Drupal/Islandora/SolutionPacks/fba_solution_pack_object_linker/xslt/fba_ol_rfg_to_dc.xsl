<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:oai_dc="http://www.openarchives.org/OAI/2.0/oai_dc/"
                xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                xmlns:dc="http://www.purl.org/dc/terms/"
                xmlns:dwc="http://rs.tdwg.org/dwc/terms/"
                xmlns:dwr="http://rs.tdwg.org/dwc/dwcrecord/"
                xmlns:dwc="http://www.fba.org.uk/objlink/">
    <xsl:output method="xml" indent="yes"/>
    <xsl:template match="/">
        <oai_dc:dc xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                       xsi:schemaLocation="http://www.openarchives.org/OAI/2.0/oai_dc/ http://www.openarchives.org/OAI/2.0/oai_dc.xsd"
                       xmlns:dc="http://purl.org/dc/elements/1.1/"
                       xmlns:oai_dc="http://www.openarchives.org/OAI/2.0/oai_dc/">
            <xsl:apply-templates exclude-result-prefixes="#all"/>
            <xsl:element name="dc:title">
                <xsl:value-of select="// objlink:title"/>
            </xsl:element>
        </oai_dc:dc>
    </xsl:template>
    <!-- suppress all else:-->
    <xsl:template match="*"/>
</xsl:stylesheet>
