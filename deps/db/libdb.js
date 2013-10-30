/** -*- js -*-
* Copyright (C) 2011 Andy Thalmann
*
* This file is part of ht4w.
*
* ht4w is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or any later version.
*
* Hypertable is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
* 02110-1301, USA.
*/

var db_static = ".\\build_windows\\db_static.vcxproj";
var db = ".\\build_windows\\db.vcxproj";
var db_vs10 = ".\\build_windows\\VS10\\db.vcxproj";
var library_props = ".\\build_windows\\library.props";
var library_props_vs10 = ".\\build_windows\\VS10\\library.props";

var libdb = ".\\build_windows\\libdb.vcxproj";
var libdb_props = ".\\build_windows\\libdb.props";
var libdb_vs10 = ".\\build_windows\\VS10\\libdb.vcxproj";
var libdb_props_vs10 = ".\\build_windows\\VS10\\libdb.props";

var xmlns = "http://schemas.microsoft.com/developer/msbuild/2003";

function prepCfg( nodes ) {
    for( var n = 0; n < nodes.length; ++n ) {
        var condition = nodes[n].getAttribute("Condition");
        if( !condition.match(/static /i) ) {
            nodes[n].parentNode.removeChild( nodes[n] );
        }
        else {
            nodes[n].setAttribute( "Condition", condition.replace(/static /gi, "") );
        }
    }
}

function prepXml( xml, hasLibraryProps ) {
    var root = xml.documentElement;

    if( hasLibraryProps ) {
        var projectConfigurations = root.selectNodes("/ns:Project/ns:ItemGroup/ns:ProjectConfiguration[@Include]");
        for( var n = 0; n < projectConfigurations.length; ++n ) {
            var include = projectConfigurations[n].getAttribute("Include");
            if (!include.match(/static /i)) {
                projectConfigurations[n].parentNode.removeChild(projectConfigurations[n]);
            }
            else {
                projectConfigurations[n].setAttribute("Include", include.replace(/static /gi, ""));
                var configuration = projectConfigurations[n].selectSingleNode("ns:Configuration");
                configuration.text = configuration.text.replace(/static /gi, "");
            }
        }

        prepCfg( root.selectNodes("/ns:Project/ns:PropertyGroup[@Condition]") );
        prepCfg( root.selectNodes("/ns:Project/ns:PropertyGroup/ns:TargetName[@Condition]") );
        prepCfg( root.selectNodes("/ns:Project/ns:PropertyGroup/ns:TargetExt[@Condition]") );
        prepCfg( root.selectNodes("/ns:Project/ns:ItemDefinitionGroup[@Condition]") );

        var libraryProps = root.selectSingleNode( "/ns:Project/ns:ImportGroup/ns:Import[@Project='library.props']" );
        if( libraryProps != null ) {
            libraryProps.setAttribute( "Project", "libdb.props" );
        }
    }

    var propertyGroups = root.selectNodes( "/ns:Project/ns:PropertyGroup[@Condition]" );
    for( var n = 0; n < propertyGroups.length; ++n ) {
        if( propertyGroups[n].getAttribute("Condition").match(/release/i) ) {
            var wpo = propertyGroups[n].selectSingleNode( "ns:WholeProgramOptimization" );
            if( wpo == null ) {
                wpo = xml.createNode( 1, "WholeProgramOptimization", xmlns ); 
                propertyGroups[n].appendChild( wpo );
            }
            wpo.text = "true";
        }
    }

    var outDirs = root.selectNodes( "/ns:Project/ns:PropertyGroup/ns:OutDir" );
    for( var n = 0; n < outDirs.length; ++n ) {
        outDirs[n].text = "$(SolutionDir)dist\\$(VisualStudioVersion)\\$(Platform)\\$(Configuration)\\libs\\";
    }

    var intDirs = root.selectNodes( "/ns:Project/ns:PropertyGroup/ns:IntDir" );
    for( var n = 0; n < intDirs.length; ++n ) {
        intDirs[n].text = "$(SolutionDir)build\\$(VisualStudioVersion)\\deps\\$(ProjectName)\\$(Platform)\\$(Configuration)\\";
    }

    var targetNames = root.selectNodes( "/ns:Project/ns:PropertyGroup/ns:TargetName" );
    for( var n = 0; n < targetNames.length; ++n ) {
        targetNames[n].text = "$(ProjectName)";
    }

    var crts = root.selectNodes( "/ns:Project/ns:ItemDefinitionGroup/ns:ClCompile/ns:RuntimeLibrary" );
    for( var n = 0; n < crts.length; ++n ) {
        if( !crts[n].text.match(/.*DLL/) ) {
            crts[n].text = crts[n].text + "DLL";
        }
    }

    var pchOuts = root.selectNodes( "/ns:Project/ns:ItemDefinitionGroup/ns:ClCompile/ns:PrecompiledHeaderOutputFile" );
    for( var n = 0; n < pchOuts.length; ++n ) {
        pchOuts[n].text = "$(IntDir)$(TargetName).pch";
    }

    var asms = root.selectNodes( "/ns:Project/ns:ItemDefinitionGroup/ns:ClCompile/ns:AssemblerListingLocation" );
    for( var n = 0; n < asms.length; ++n ) {
        asms[n].text = "$(IntDir)";
    }

    var objs = root.selectNodes( "/ns:Project/ns:ItemDefinitionGroup/ns:ClCompile/ns:ObjectFileName" );
    for( var n = 0; n < objs.length; ++n ) {
        objs[n].text = "$(IntDir)";
    }

    var clCompiles = root.selectNodes( "/ns:Project/ns:ItemDefinitionGroup/ns:ClCompile" );
    for( var n = 0; n < clCompiles.length; ++n ) {
        var pdbfn = clCompiles[n].selectSingleNode( "ns:ProgramDataBaseFileName" );
        if( pdbfn == null ) {
            pdbfn = xml.createNode( 1, "ProgramDataBaseFileName", xmlns ); 
            clCompiles[n].appendChild( pdbfn );
        }
        pdbfn.text = "$(OutDir)$(TargetName).pdb";

        var condition = clCompiles[n].parentNode.getAttribute("Condition");
        if( condition != null && condition.match(/release/i) ) {
            var eeis = clCompiles[n].selectSingleNode( "ns:EnableEnhancedInstructionSet" );
            if( eeis == null ) {
                eeis = xml.createNode( 1, "EnableEnhancedInstructionSet", xmlns );
                clCompiles[n].appendChild( eeis );
            }
            eeis.text = "StreamingSIMDExtensions2";

            var mr = clCompiles[n].selectSingleNode( "ns:MinimalRebuild" );
            if( mr == null ) {
                mr = xml.createNode( 1, "MinimalRebuild", xmlns ); 
                clCompiles[n].appendChild( mr );
            }
            mr.text = "false";
        }
    }
}

function prep() {
    try {
        var hasLibraryProps = false;
        var xml = new ActiveXObject( "Msxml2.DOMDocument.3.0" );
        xml.async = false;
        xml.load( db_static );
        if( xml.parseError.errorCode != 0 ) {
            xml.load( db_vs10 );
            if( xml.parseError.errorCode != 0 ) {
                xml.load( db );
                if( xml.parseError.errorCode != 0 ) {
                    WScript.Echo("loading " + db_static + ", " + db_vs10 + " or " + db + " failed (" + xml.parseError.reason + ")");
                    return;
                }
            }
            else {
                libdb = libdb_vs10;
            }
            hasLibraryProps = true;
        }
        xml.setProperty( "SelectionLanguage", "XPath" );
        xml.setProperty( "SelectionNamespaces", "xmlns:ns='" + xmlns + "'" );

        prepXml( xml, hasLibraryProps );
        xml.save( libdb );

        if( hasLibraryProps ) {
            var xml = new ActiveXObject( "Msxml2.DOMDocument.3.0" );
            xml.async = false;
            xml.load( library_props_vs10 );
            if( xml.parseError.errorCode != 0 ) {
                xml.load( library_props );
            }
            else {
                libdb_props = libdb_props_vs10;
            }
            if( xml.parseError.errorCode == 0 ) {
                xml.setProperty( "SelectionLanguage", "XPath" );
                xml.setProperty( "SelectionNamespaces", "xmlns:ns='" + xmlns + "'" );

                prepXml( xml, true );
                xml.save( libdb_props );
            }
        }
        WScript.Echo( libdb + " has been sucessfully created" );
    }
    catch( e ) {
        WScript.Echo( "libdb failed: " + e.description );
    }
}

prep();
