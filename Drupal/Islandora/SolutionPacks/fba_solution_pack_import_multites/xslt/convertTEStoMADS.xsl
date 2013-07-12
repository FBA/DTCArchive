<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns="http://www.loc.gov/mads/v2"
                xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                xmlns:xs="http://www.w3.org/2001/XMLSchema"
                xsi:schemaLocation="http://www.loc.gov/mads/v2 http://www.loc.gov/standards/mads/mads.xsd"
                exclude-result-prefixes="xs"
                version="1.0">
    <xsl:output indent="yes"/>
    <xsl:template match="/">
        <madsCollection xmlns="http://www.loc.gov/mads/v2"
                        xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                        xsi:schemaLocation="http://www.loc.gov/mads/v2 http://www.loc.gov/standards/mads/mads.xsd">

            <xsl:for-each select="//CONCEPT[DESCRIPTOR]">
                <xsl:variable name="value">
                    <xsl:value-of select="DESCRIPTOR"/>
                </xsl:variable>
                <mads>
                    <authority>
                        <topic>
                            <xsl:value-of select="$value"/>
                        </topic>
                    </authority>
                    <xsl:for-each select="BT">
                        <related type="broader">
                            <topic>
                                <xsl:value-of select="."/>
                            </topic>
                        </related>
                    </xsl:for-each>
                    <xsl:for-each select="NT">
                        <related type="narrower">
                            <topic>
                                <xsl:value-of select="."/>
                            </topic>
                        </related>
                    </xsl:for-each>
                    <xsl:for-each select="RT">
                        <related>
                            <topic>
                                <xsl:value-of select="."/>
                            </topic>
                        </related>
                    </xsl:for-each>
                    <xsl:for-each select="PU">
                        <related type="other" otherType="preferred unit">
                            <topic>
                                <xsl:value-of select="."/>
                            </topic>
                        </related>
                    </xsl:for-each>
                    <xsl:for-each select="UF">
                        <related type="other" otherType="used for">
                            <topic>
                                <xsl:value-of select="."/>
                            </topic>
                        </related>
                    </xsl:for-each>
                    <xsl:for-each select="DF">
                        <note type="definition">
                            <xsl:value-of select="."/>
                        </note>
                    </xsl:for-each>
                    <xsl:for-each select="SD">
                        <note type="definition source">
                            <xsl:value-of select="."/>
                        </note>
                    </xsl:for-each>
                    <xsl:for-each select="SN">
                        <note type="scope">
                            <xsl:value-of select="."/>
                        </note>
                    </xsl:for-each>
                    <xsl:for-each select="STA">
                        <note type="validity">
                            <xsl:value-of select="."/>
                        </note>
                    </xsl:for-each>
                    <xsl:for-each select="APP">
                        <note type="approved date">
                            <xsl:value-of select="."/>
                        </note>
                    </xsl:for-each>
                    <recordInfo>
                        <recordCreationDate>
                            <xsl:value-of select="INP"/>
                        </recordCreationDate>
                        <recordChangeDate>
                            <xsl:value-of select="UPD"/>
                        </recordChangeDate>
                    </recordInfo>
                    <identifier type="TNR">
                        <xsl:value-of select="TNR"/>
                    </identifier>
                </mads>
            </xsl:for-each>
            <xsl:for-each select="//CONCEPT[NON-DESCRIPTOR]">
                <xsl:variable name="value">
                    <xsl:value-of select="NON-DESCRIPTOR"/>
                </xsl:variable>
                <mads>
                    <variant>
                        <topic>
                            <xsl:value-of select="$value"/>
                        </topic>
                    </variant>
                    <xsl:for-each select="USE">
                        <related type="other" otherType="use">
                            <topic>
                                <xsl:value-of select="."/>
                            </topic>
                        </related>
                    </xsl:for-each>
                    <xsl:for-each select="APP">
                        <note type="approved date">
                            <xsl:value-of select="."/>
                        </note>
                    </xsl:for-each>
                    <recordInfo>
                        <recordCreationDate>
                            <xsl:value-of select="INP"/>
                        </recordCreationDate>
                        <recordChangeDate>
                            <xsl:value-of select="UPD"/>
                        </recordChangeDate>
                    </recordInfo>
                    <identifier type="TNR">
                        <xsl:value-of select="TNR"/>
                    </identifier>
                </mads>
            </xsl:for-each>
        </madsCollection>
    </xsl:template>
</xsl:stylesheet>