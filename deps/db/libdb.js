var vcxproj = ".\\build_windows\\db_static.vcxproj";
var libdb = ".\\build_windows\\libdb.vcxproj";
var xmlns = "http://schemas.microsoft.com/developer/msbuild/2003";


function prep() {
    try {
        var xml = new ActiveXObject( "Msxml2.DOMDocument.3.0" );
        xml.async = false;        
        xml.load( vcxproj );        
        if( xml.parseError.errorCode != 0 ) {
            WScript.Echo( "loading " + vcxproj + "failed (" + xml.parseError.reason + ")" );
            return;
        }
        xml.setProperty( "SelectionLanguage", "XPath" );
        xml.setProperty( "SelectionNamespaces", "xmlns:ns='" + xmlns + "'" );

        var root = xml.documentElement;        
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

        var outDirs = root.selectNodes( "/ns:Project/ns:PropertyGroup/ns:OutDir[@Condition]" );
        for( var n = 0; n < outDirs.length; ++n ) {
            outDirs[n].text = "$(SolutionDir)dist\\$(Platform)\\$(Configuration)\\libs\\";
        }
        
        var intDirs = root.selectNodes( "/ns:Project/ns:PropertyGroup/ns:IntDir[@Condition]" );
        for( var n = 0; n < intDirs.length; ++n ) {
            intDirs[n].text = "$(SolutionDir)build\\deps\\$(ProjectName)\\$(Platform)\\$(Configuration)\\";
        }
        
        var targetNames = root.selectNodes( "/ns:Project/ns:PropertyGroup/ns:TargetName[@Condition]" );
        for( var n = 0; n < targetNames.length; ++n ) {
            targetNames[n].text = "$(ProjectName)";
        }            

        var crts = root.selectNodes( "/ns:Project/ns:ItemDefinitionGroup[@Condition]/ns:ClCompile/ns:RuntimeLibrary" );
        for( var n = 0; n < crts.length; ++n ) {
            crts[n].text = crts[n].text + "DLL";
        }
        
        var pchOuts = root.selectNodes( "/ns:Project/ns:ItemDefinitionGroup[@Condition]/ns:ClCompile/ns:PrecompiledHeaderOutputFile" );
        for( var n = 0; n < pchOuts.length; ++n ) {
            pchOuts[n].text = "$(IntDir)$(TargetName).pch";
        }
        
        var asms = root.selectNodes( "/ns:Project/ns:ItemDefinitionGroup[@Condition]/ns:ClCompile/ns:AssemblerListingLocation" );
        for( var n = 0; n < asms.length; ++n ) {
            asms[n].text = "$(IntDir)";
        }
        
        var objs = root.selectNodes( "/ns:Project/ns:ItemDefinitionGroup[@Condition]/ns:ClCompile/ns:ObjectFileName" );
        for( var n = 0; n < objs.length; ++n ) {
            objs[n].text = "$(IntDir)";
        }
        
        var clCompiles = root.selectNodes( "/ns:Project/ns:ItemDefinitionGroup[@Condition]/ns:ClCompile" );
        for( var n = 0; n < clCompiles.length; ++n ) {
            var pdbfn = clCompiles[n].selectSingleNode( "ns:ProgramDataBaseFileName" );  
            if( pdbfn == null ) {
                pdbfn = xml.createNode( 1, "ProgramDataBaseFileName", xmlns ); 
                clCompiles[n].appendChild( pdbfn );
            }
            pdbfn.text = "$(OutDir)$(TargetName).pdb";
            
            if( clCompiles[n].parentNode.getAttribute("Condition").match(/release/i) ) {                
                var eeis = clCompiles[n].selectSingleNode( "ns:EnableEnhancedInstructionSet" );  
                if( eeis == null ) {
                    eeis = xml.createNode( 1, "EnableEnhancedInstructionSet", xmlns ); 
                    clCompiles[n].appendChild( eeis );
                }
                eeis.text = "StreamingSIMDExtensions2";
				
				var mr = clCompiles[n].selectSingleNode( "ns:MinimalRebuild" );  
                if( mr == null ) {
                    mr = xml.createNode( 1, "MinimalRebuild", xmlns ); 
                    clCompiles[n].appendChild( mr  );
                }
                mr.text = "false";
            }
        } 

        xml.save( libdb );        
        WScript.Echo( libdb + " has been sucessfully created" );
    }
    catch( e ) {
        WScript.Echo( "libdb failed: " + e.description );
    }
}

prep();
    