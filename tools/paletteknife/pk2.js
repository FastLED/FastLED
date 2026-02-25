var stdout = "";

function cursor_wait() {
    document.body.style.cursor = 'wait';
}

function cursor_clear() {
    document.body.style.cursor = 'default';
}

cursor_wait();

function getParameterByName(name) {
 var match = RegExp('[?&]' + name + '=([^&]*)').exec(window.location.search);
 return match && decodeURIComponent(match[1].replace(/\+/g, ' '));
}
 
function isNumeric(o)
{
    return !isNaN(parseFloat(o)) && isFinite(o);
}
 
function out()
{
    var args = Array.prototype.slice.call(arguments, 0);
    //    document.getElementById('output').innerHTML += args.join(" ") + "\n";
    stdout += args.join(" ") + "\n"; 
}


function adjustGamma( orig, gamma)
{
    var o = orig / 255.0;
    var adj = Math.pow( o, gamma);
    var res = Math.floor( adj * 255.0);
    if( (orig != 0) && (res == 0)) {
	res = 1;
    }
    return res;
}


var origurl = document.location.href;
if (document.location.href.endsWith(".png.index.html"))
{
  // prev: "http://seaviewsensing.com/pub/cpt-city/dca/tn/alarm.p1.0.2.png.index.html";
  //    -> "http://seaviewsensing.com/pub/cpt-city/dca/alarm.p1.0.2.c3g";
  var url2 = origurl.replace( "/tn/", "/");
  var url3 = url2.replace( ".png.index.html", ".c3g");
  var matches = url3.match("([^/]+)\.c3g");
  var nom = matches[1];

} else {
  // now:  "https://phillips.shef.ac.uk/pub/cpt-city/dca/alarm-p1-0-2"
  //       the 4th link on the page leads to the c3g file
  var nom = document.location.pathname.replace( "/pub/cpt-city/", "");
  var url3 = document.querySelectorAll('div.scheme a')[0].href;
}
nom = nom.replace(/[^A-Za-z0-9]/g,"_");
nom = nom + "_gp";
console.log("nom: " + nom);
console.log("url3: " + url3);



var src = "/*\n\
   CSS3 gradient\n\
   cptutils 1.55\n\
   Mon Sep  1 00:02:16 2014\n\
*/\n\
\n\
linear-gradient(\n\
  0deg,\n\
  rgb(186,218,139)   0.000%,\n\
  rgb(186,218,139)   3.330%,\n\
  rgb(231,234,155)   3.330%,\n\
  rgb(231,234,155)   8.330%,\n\
  rgb(254,243,181)   8.330%,\n\
  rgb(254,243,181)  16.670%,\n\
  rgb(246,215,150)  16.670%,\n\
  rgb(246,215,150)  25.000%,\n\
  rgb(205,164,119)  25.000%,\n\
  rgb(205,164,119)  33.330%,\n\
  rgb(187,144,105)  33.330%,\n\
  rgb(187,144,105)  50.000%,\n\
  rgb(225,225,225)  50.000%,\n\
  rgb(225,225,225) 100.000%\n\
  );";


var rgamma = 2.6;
var ggamma = 2.2;
var bgamma = 2.5;

var garg = getParameterByName("gamma");
if( isNumeric( garg)) { rgamma=garg; ggamma=garg; bgamma=garg; }
var rgarg = getParameterByName("rgamma");
if( isNumeric( rgarg)) { rgamma = rgarg; }
var ggarg = getParameterByName("ggamma");
if( isNumeric( ggarg)) { ggamma = ggarg; }
var bgarg = getParameterByName("bgamma");
if( isNumeric( bgarg)) { bgamma = bgarg; }

function CreateConversion( src)
{
    stdout="";

    if( src.indexOf("rgba(") > -1 ) {
	alert("WARNING: TRANSPARENCY not supported.");
    }

    var segs = 0;
    var lines = src.split('\n');

    out("DEFINE_GRADIENT_PALETTE( " + nom + " ) {");
    
    for(var i = 0;i < lines.length;i++){
	var line = lines[i]
	    
	//out(line);
	var nums = line.match("([0-9]+), *([0-9]+), *([0-9]+)\\) *([0-9.]+)");
	if( nums ) {
	    //out( nums);
	    var r = nums[1];
	    var g = nums[2];
	    var b = nums[3];
	    var pct = nums[4];
	    var ndx = Math.floor( (pct * 255.0) / 100.0 );
	    
	    r = adjustGamma( r, rgamma);
	    g = adjustGamma( g, ggamma);
	    b = adjustGamma( b, bgamma);
	    
	    var rstr = ("  " + r).slice(-3);
	    var gstr = ("  " + g).slice(-3);
	    var bstr = ("  " + b).slice(-3);
	    var ndxstr = ("  " + ndx).slice(-3);
	    
	    var outstr = "  ";
	    outstr += ndxstr + ", ";
	    outstr += rstr + ",";
	    outstr += gstr + ",";
	    outstr += bstr;
	    
	    if( ndx == 255 ) {
		outstr += "};";
	    } else {
		outstr += ",";
	    }
	    out( outstr);
	    segs++;
	}
    }

    var prologue = "";
    prologue += ("// Gradient palette \"" + nom + "\", originally from\n");
    prologue += ("// " + origurl + "\n");
    prologue += ("// converted for FastLED with gammas (" + rgamma + ", " + ggamma + ", " + bgamma + ")\n");
    prologue += ("// Size: " + ((segs)* 4) + " bytes of program space.\n");

    if( segs > 50) {
	alert("NOTE: this is a BIG one (" + (segs) + " control points, " +((segs) * 4) + " bytes), and some colors may not show.");
    } else if( segs > 16) {
	alert("NOTE: >16 control points (" + (segs) + "), some colors may not show.");
    }


    return prologue + "\n" + stdout;
}

function DoConvert( content)
{
    var res = CreateConversion( content);
    window.prompt("COPY this code, paste into your FastLED sketch:",res);
    
}



// var onSite = url3.indexOf("https://phillips.shef.ac.uk/");
// if( onSite != 0) {
//     window.location.href="https://phillips.shef.ac.uk/pub/cpt-city/";
//     //return;
// } else {
  fetch(url3)
    .then((response) => {
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      return response.text();
    })
    .then((content) => {
      DoConvert(content);
    })
    .catch((error) => {
      console.error("Error converting: ", error);
    });
// }

cursor_clear();
