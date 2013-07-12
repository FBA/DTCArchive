<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:mods="http://www.loc.gov/mods/v3" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
    <xsl:output method="xml" indent="yes" />
    <!-- Root element -->
    <!--   /DATABASE_FBA when more than one RECORD elements in xml file -->
    <xsl:template match="/DATABASE_FBA">
        <mods:modsCollection xsi:schemaLocation="http://www.loc.gov/mods/v3 http://www.loc.gov/standards/mods/v3/mods-3-0.xsd">
            <xsl:apply-templates select="RECORD[translate(normalize-space(Document_type/text()), '.', '') != 'T']" mode="record" />
        </mods:modsCollection>
    </xsl:template>

    <!-- Root element -->
    <!--   /RECORD when only 1 RECORD element in xml file -->
    <xsl:template match="/RECORD">
        <xsl:apply-templates select="../RECORD[translate(normalize-space(Document_type/text()), '.', '') != 'T']" mode="record" />
    </xsl:template>

    <!-- Build Common Elements -->
    <xsl:template match="RECORD" mode="record">
        <!-- Document type used at various places in template -->
        <xsl:variable name="sDocType" select="translate(normalize-space(Document_type/text()), '.', '')" />

        <mods:mods>
            <mods:identifier type="mfn">
                <xsl:value-of select="Mfn" />
            </mods:identifier>

            <mods:recordInfo>
                <xsl:apply-templates select="Date_of_entry" />
                <mods:recordContentSource>FBA Library Catalogue</mods:recordContentSource>
            </mods:recordInfo>

            <mods:note type="doc type">Monograph</mods:note> <!--on the same line to eliminate spaces -->
            
            <!-- Authors -->
            <xsl:apply-templates select="MONOGRAPHIC_LEVEL_author_s_">
                <xsl:with-param name="sType">personal</xsl:with-param>
                <xsl:with-param name="sRole">
                    <xsl:value-of select="Author_role" />
                </xsl:with-param>
                <xsl:with-param name="sDocType">
                    <xsl:value-of select="$sDocType" />
                </xsl:with-param>
            </xsl:apply-templates>
            <xsl:apply-templates select="Corporate_author_s_">
                <xsl:with-param name="sType">corporate</xsl:with-param>
                <xsl:with-param name="sRole">Author</xsl:with-param>
                <xsl:with-param name="sDocType">
                    <xsl:value-of select="$sDocType" />
                </xsl:with-param>
            </xsl:apply-templates>
            <!-- End Authors -->

            <!-- Title -->
            <xsl:apply-templates select="Title__monogr__" />
        
            <!-- Annotations -->
            <xsl:apply-templates select="Annotation" />
        
            <!-- Windermere Shelfmark -->
            <xsl:apply-templates select="Windermere_shelfmark" />
        
            <!-- Dorset Shelfmark -->
            <xsl:apply-templates select="Dorset_shelfmark" />
        
            <!-- Keywords (New Element Added By Our Custom Processor To Insert Keyword Strings For Indexing Element) -->
            <xsl:apply-templates select="Keywords" />

            <!-- CategoryMapped -->
            <xsl:apply-templates select="Category_mapped" />
        
            <!-- Search Terms -->
            <xsl:apply-templates select="Search_terms" />               
            
            <!-- Document type specific elements -->
            <xsl:apply-templates select="." mode="B" />
            <!-- End Document type specific elements --> 

            <mods:extension>   
                <xsl:apply-templates select="Date_of_publication" >
                    <xsl:with-param name="destination">extension</xsl:with-param>
                </xsl:apply-templates>
                <xsl:apply-templates select="Imprint__Place__Publ___Date_" >            
                    <xsl:with-param name="destination">extension</xsl:with-param>
                </xsl:apply-templates>            
            </mods:extension>
            <xsl:apply-templates select="Number_of_copies" />                        
            <xsl:apply-templates select="BL_index" />  
            <xsl:apply-templates select="Water_authority_mapped" />   
            <mods:classification authority="fba" displayLabel="FBA Legacy Keyword Strings">
                <xsl:apply-templates select="Indexing" />
            </mods:classification>
        </mods:mods>
    
    </xsl:template>

    <!-- Document Type B Specific Layout -->
    <xsl:template match="RECORD" mode="B">
        <mods:originInfo>
            <mods:issuance>monographic</mods:issuance>
            <xsl:apply-templates select="Edition" />
            <xsl:apply-templates select="Imprint__Place__Publ___Date_" >
                <xsl:with-param name="destination">originInfo</xsl:with-param>
            </xsl:apply-templates>        
        </mods:originInfo>
        <mods:typeOfResource>text</mods:typeOfResource>
        <xsl:apply-templates select="Pages" mode="physicalDesc" />
        <xsl:apply-templates select="Other_pagination[not(../Pages)]" mode="physicalDesc" />
        <xsl:apply-templates select="ISBN" />
    </xsl:template>
    <!-- End Document Type B Specific Layout -->

    <xsl:template match="Date_of_entry">
        <mods:recordCreationDate>
            <xsl:value-of select="normalize-space(text())" />
        </mods:recordCreationDate>                    
    </xsl:template>

    <xsl:template match="Annotation">
        <mods:note type="annotation">
            <xsl:apply-templates select="text()" mode="trim" />
        </mods:note>   
    </xsl:template>

    <xsl:template match="MONOGRAPHIC_LEVEL_author_s_ | Monographic_source_author_s_">
        <xsl:param name="sType" />
        <xsl:param name="sRole" />
        <xsl:param name="sDocType" />
        <mods:name >
            <xsl:attribute name="type">
                <xsl:value-of select="$sType" />
            </xsl:attribute>
            <mods:namePart type="family">
                <xsl:value-of select="substring-before(normalize-space(text()),'@@')" />
            </mods:namePart>
            <mods:namePart type="given">
                <xsl:value-of select="substring-after(normalize-space(text()),'@@')" />
            </mods:namePart>
            <mods:role>
                <mods:roleTerm authority="marcrelator">
                    <xsl:value-of select="$sRole" />
                </mods:roleTerm>
            </mods:role>
        </mods:name>
    </xsl:template>

    <xsl:template match="Corporate_author_s_ | Corporate_source_author_s_">
        <xsl:param name="sType" />
        <xsl:param name="sRole" />
        <xsl:param name="sDocType" />
        <mods:name>
            <xsl:attribute name="type">
                <xsl:value-of select="$sType" />
            </xsl:attribute>
            <mods:namePart>
                <xsl:value-of select="normalize-space(text())" />
            </mods:namePart>
            <mods:role>
                <mods:roleTerm type="text">
                    <xsl:value-of select="$sRole" />
                </mods:roleTerm>
            </mods:role>
        </mods:name>
        
    </xsl:template>

    <xsl:template match="Title | Serial_Journal_title | Title__monogr__">
        <mods:titleInfo>
            <mods:title>
                <xsl:value-of select="normalize-space(text())" />
                <!-- xsl:apply-templates select="text()" mode="trim" / -->
            </mods:title>
        </mods:titleInfo>
    </xsl:template>

    <xsl:template match="Windermere_shelfmark">
        <mods:location displayLabel="physical">
            <mods:physicalLocation>FBA Windermere</mods:physicalLocation>
            <mods:shelfLocator>
                <xsl:apply-templates select="." mode="trim" />
            </mods:shelfLocator>
        </mods:location>
    </xsl:template>

    <xsl:template match="Dorset_shelfmark">
        <mods:location displayLabel="physical">
            <mods:physicalLocation>FBA Dorset</mods:physicalLocation>
            <mods:shelfLocator>
                <xsl:apply-templates select="." mode="trim" />
            </mods:shelfLocator>
        </mods:location>
    </xsl:template>

    <xsl:template match="BL_index">
        <mods:subject authority="british library index">
            <mods:geographic>
                <xsl:apply-templates select="." mode="trim" />
            </mods:geographic>
        </mods:subject>
    </xsl:template> 

    <xsl:template match="Water_authority_mapped">
        <mods:subject authority="water authority">
            <mods:name type="corporate">
                <mods:namePart>
                    <xsl:apply-templates select="." mode="trim" />
                </mods:namePart>
            </mods:name>
        </mods:subject>
    </xsl:template> 

    <xsl:template match="Indexing"> 
        <xsl:value-of select="normalize-space(concat(text(),';'))" />
        <!-- xsl :apply-templates select="text()" mode="trim" / -->
    </xsl:template>

    <xsl:template match="Keywords">
        <xsl:apply-templates select="Keyword" />
    </xsl:template>

    <xsl:template match="Keyword">
        <mods:subject authority="fba">
            <mods:topic>
                <xsl:apply-templates select="." mode="trim" />
            </mods:topic>
        </mods:subject>    
    </xsl:template>

    <xsl:template match="Search_terms">
        <mods:subject authority="asfa">
            <mods:geographic>
                <xsl:apply-templates select="." mode="trim" />
            </mods:geographic>
        </mods:subject>    
    </xsl:template>

    <xsl:template match="Category_mapped">
        <mods:identifier type="special collection">
            <xsl:apply-templates select="." mode="trim" />
        </mods:identifier>
    </xsl:template>

    <xsl:template match="Imprint__Place__Publ___Date_">
        <xsl:param name="destination" />
        <xsl:if test="$destination = 'originInfo' ">
            <xsl:call-template name="tokenize">
                <xsl:with-param name="string" select="Imprint__Place__Publ___Date__a" />
            </xsl:call-template>
        </xsl:if> 	   
        <xsl:apply-templates select="Imprint__Place__Publ___Date__b" >
            <xsl:with-param name="destination" select="$destination" />
        </xsl:apply-templates>                         	
    </xsl:template>

    <xsl:template name="tokenize">
        <xsl:param name="string" />
        <xsl:param name="delimiter" select="','" />
        <xsl:param name="sPlace"></xsl:param>
        <xsl:choose>
            <xsl:when test="$delimiter and contains($string, $delimiter)">
                <xsl:call-template name="tokenize">
                    <xsl:with-param name="string" select="substring-after($string, $delimiter)" />
                    <xsl:with-param name="delimiter" select="$delimiter" />
                    <xsl:with-param name="sPlace">
                        <xsl:value-of select="$sPlace" />
                        <xsl:if test="string-length($sPlace) > 0">
                            <xsl:value-of select="', '" />
                        </xsl:if>
                        <xsl:value-of select="substring-before($string, $delimiter)" />
                    </xsl:with-param>
                </xsl:call-template>
            </xsl:when>
            <xsl:otherwise>
                <xsl:if test="string-length($sPlace) > 0">
                    <mods:place>
                        <mods:placeTerm type="text">
                            <xsl:value-of select="normalize-space($sPlace)" />
                        </mods:placeTerm>
                    </mods:place>
                </xsl:if>
                <mods:publisher>
                    <xsl:value-of select="normalize-space($string)" />
                </mods:publisher>
            </xsl:otherwise>
        </xsl:choose>

    </xsl:template>

    <xsl:template match="Imprint__Place__Publ___Date__b">
        <xsl:param name="destination" />    
        <xsl:choose>
            <xsl:when test="$destination = 'originInfo' ">
                <xsl:if test="not(string(number(text()))='NaN')">
                    <xsl:if test="text() &gt; 1686 and text() &lt; 3000">           
                        <mods:dateIssued>      
                            <xsl:call-template name="FormatDateYYYYToSolrDate" >
                                <xsl:with-param name="DateTime" select="text()" />        
                            </xsl:call-template>          
                        </mods:dateIssued>
                    </xsl:if>             
                </xsl:if>              
            </xsl:when>
            <xsl:otherwise>
                <xsl:choose>            
                    <xsl:when test="$destination = 'extension' ">
                        <mods:dateOfPublication>
                            <xsl:apply-templates select="text()" mode="trim" />
                        </mods:dateOfPublication>
                    </xsl:when>
                </xsl:choose>             
            </xsl:otherwise>           
        </xsl:choose>        
    </xsl:template>

    <xsl:template match="Date_of_publication">
        <xsl:param name="destination" />
        <xsl:choose>
            <xsl:when test="$destination = 'relatedItem'">
                <xsl:if test="not(string(number(text()))='NaN')">
                    <xsl:if test="text() &gt; 1686 and text() &lt; 3000">          
                        <mods:dateIssued>      
                            <xsl:call-template name="FormatDateYYYYToSolrDate" >
                                <xsl:with-param name="DateTime" select="text()" />        
                            </xsl:call-template>          
                        </mods:dateIssued>
                    </xsl:if>              
                </xsl:if>              
            </xsl:when>
            <xsl:otherwise>
                <xsl:choose>                     
                    <xsl:when test="$destination = 'extension'">
                        <mods:dateOfPublication>
                            <xsl:apply-templates select="text()" mode="trim" />
                        </mods:dateOfPublication>
                    </xsl:when>
                </xsl:choose>           
            </xsl:otherwise>                 
        </xsl:choose>               
    </xsl:template>

    <xsl:template match="Collation | Extent_of_chapter | Other_pagination" mode="part">
        <mods:part>
            <xsl:apply-templates select="Collation_a" />
            <xsl:apply-templates select="Collation_b" />
            <xsl:apply-templates select="Collation_c | ../Extent_of_chapter" />
            <xsl:apply-templates select="../Other_pagination | Other_pagination" />
        </mods:part>
    </xsl:template>

    <xsl:template match="Collation_a">
        <mods:detail type="volume">
            <mods:number>
                <xsl:value-of select="text()" />
            </mods:number>
        </mods:detail>
    </xsl:template>

    <xsl:template match="Collation_b">
        <mods:detail type="issue">
            <mods:number>
                <xsl:value-of select="text()" />
            </mods:number>
        </mods:detail>
    </xsl:template>

    <xsl:template match="Collation_c | Extent_of_chapter">
        <mods:extent unit="pages">
            <mods:list>
                <xsl:apply-templates select="text()" mode="trim" />
            </mods:list>
        </mods:extent>
    </xsl:template>

    <xsl:template match="Other_pagination">
        <mods:extent unit="other pagination">
            <mods:list>
                <xsl:apply-templates select="text()" mode="trim" />
            </mods:list>
        </mods:extent>
    </xsl:template>

    <xsl:template match="Pages" mode="physicalDesc">
        <mods:physicalDescription lang="eng">
            <xsl:apply-templates select="../Pages" mode="extent" />
        </mods:physicalDescription>
    </xsl:template>

    <xsl:template match="Other_pagination" mode="physicalDesc">
        <mods:physicalDescription>
            <xsl:apply-templates select="../Other_pagination" mode="extent" />
        </mods:physicalDescription>
    </xsl:template>

    <xsl:template match="Pages | Other_pagination" mode="extent">
        <mods:extent>
            <xsl:apply-templates select="text()" mode="trim" />
        </mods:extent>
    </xsl:template>

    <xsl:template match="Edition">
        <mods:edition>
            <xsl:apply-templates select="text()" mode="trim" />
        </mods:edition>
    </xsl:template>

    <xsl:template match="ISBN">
        <mods:identifier type="isbn">
            <xsl:apply-templates select="text()" mode="trim" />
        </mods:identifier>
    </xsl:template>

    <xsl:template match="Number_of_copies">
        <mods:note type="number of copies">
            <xsl:value-of select="text()" />
        </mods:note>
    </xsl:template>

    <!-- Utility Template used to trim trailing space and . from text elements -->
    <xsl:template match="text()" mode="trim">
        <xsl:variable name="sString">
            <xsl:value-of select="normalize-space(.)" />
        </xsl:variable>
        <xsl:variable name="sNewString">
            <xsl:choose>
                <xsl:when test="substring($sString,string-length($sString)) = '.'">
                    <xsl:value-of select="substring($sString,1,string-length($sString)-1)"/>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:value-of select="$sString"/>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:variable>
        <xsl:value-of select="$sNewString" />
    </xsl:template>



    <xsl:template name="FormatDateYYYYToSolrDate">
        <xsl:param name="DateTime" />
        <xsl:variable name="year" select="substring($DateTime,1,4)" />
        <xsl:variable name="mo" select="'01'" />
        <xsl:variable name="day">01</xsl:variable>                
        <xsl:variable name="hh">00</xsl:variable>
        <xsl:variable name="mm">00</xsl:variable>
        <xsl:variable name="ss">00</xsl:variable>

        <xsl:value-of select="$year" />
        <xsl:value-of select="'-'"/>
        <xsl:value-of select="$mo"/>
        <xsl:value-of select="'-'"/> 
        <xsl:value-of select="$day"/>
        <xsl:value-of select="'T'"/>
        <xsl:value-of select="$hh"/>
        <xsl:value-of select="':'"/>
        <xsl:value-of select="$mm"/>
        <xsl:value-of select="':'"/>
        <xsl:value-of select="$ss"/> 
    </xsl:template>
</xsl:stylesheet>