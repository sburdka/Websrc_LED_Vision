
var on=1;
debugger;
var localstorage = window.localStorage;
var SOUNDFLAG = 'sound';

function setSoundFlag(argument) {
	// body...
	localstorage.setItem(SOUNDFLAG , argument);
}

function getSoundFlag() {
	// body...
	debugger;
	if(localstorage.getItem(SOUNDFLAG)){

	}else{
		localstorage.setItem(SOUNDFLAG , 1);
	}
	return localstorage.getItem(SOUNDFLAG);
}

function giveSound() {
      var d = getSoundFlag();
      $.ajax({
         url: "/api/sound?data="+d,
         type: "GET",
         cache: false,
         success: function (response) {}
     });
}

function remote(remote_value)
{
 switch(remote_value)
 {
  case  65:
		if(on==0)
		{
			on=1;
			$.ajax({
                     url: "/api/hdmi",
                     data: {on:"ON"},
                     type: "GET",
                     cache: false,
                     success: function (response) {}
                 });
		}
		else
		{
			on=0;
			$.ajax({
                     url: "/api/hdmi",
                     data: {off:"OFF"},
                     type: "GET",
                     cache: false,
                     success: function (response) {}
                 });

		}

 		break; //HDMI ON/OFF
  case  66: window.location="usb_import_export.html";
		
		break; //HELP
  case  67: 
  		debugger;
  		var a = parseInt(getSoundFlag());
  		if(a == 1)
  			setSoundFlag(0);
  		else 
  			setSoundFlag(1);
  		var s  = getSoundFlag();
  		getSoundFlag();
  			myFunction();
		break; //MUTE
  case  68: 
		window.location="textpage_new.html?page=0";
		break; //LANDOLT C
  case  70: 
		window.location="textpage_new.html?page=1";
		break; //THUMBLING E
  case  71: 
		window.location="textpage_new.html?page=2";
		break; //ENGLISH
  case  72: 
		window.location="textpage_new.html?page=3";
		break; //NUMBERS
  case  76: 
		window.location="canva.html";
		break; //SNELLEN
  case  77: 
		window.location="ped.html";
		break; //PEDIATRIC
  case  78: 
		 window.location="log.html";
		break; //ETDRS
  case  79: 
	
		break; //LOGMAR
  case  80: 
		window.location="textpage_new.html?page=4";
		break; //LANG 1
  case  81: 
		window.location="textpage_new.html?page=5";
		break; //LANG 2
  case  82: 
		window.location="textpage_new.html?page=6";
		break; //LANG 3
  case  83: break; //LANG 4
  case  84: 
		window.location="duo_chrome.html";
		break; //DUOCHROME
  case  85: 
		window.location="settingsmain.html";
		break; //SETTING
  case  86: 
		window.location="Home.html";
		break; //EXIT OR HOME
  case  87: 
		window.location="grid.html?id=0";
		break; //AMSLER GRID
  case  88: 
		window.location="color_dots.html";
		break; //WORTH 4 DOTS
  case  90: 
		window.location="illitrate dots.html";
		break; //ILLUSTRATED DOTS
  case 190: 
	debugger;
if(localstorage.getItem('feetvalue')){

	}else{
		localstorage.setItem('feetvalue' , '8');
	}
var temp = localstorage.getItem('feetvalue');

		if(temp == "6" || temp == "7"){
		window.location="support.html";
		}
else{
window.location="line_move.html";
}
		break; //VERNIER ACTIVITY
  case  32: 
		window.location="maddox.html";
		break; //MADDOX
  case  89: 
		window.location="grid.html?id="+1;
		break; //PHORIA
  case  73: 
		window.location="speedometer.html";		
		break; //ASTIGLAN
  case 188: 
		window.location="dotimg.php.html";
		break; //ISHIARA
  case  49: 
		window.location="grid.html?id="+3;
		break; //H-COIN
  case  50: 
		window.location="grid.html?id="+4;
		break; //V-COIN
  case  51: 
		window.location="textpage_new.html?page=18";
		break; //H-MASK
  case  52: 
		window.location="textpage_new.html?page=19";
		break; //V-MASK
  case  53: 
		window.location="COLOR_CONTRAST.html";
		break; //CONTRAST SENSITIVITY
  case  54: 
		window.location="anatomy_front.html";
		break; //EDU-CHART
  case  55: 
		
		break; //MULTILINE
  case  56: 
		debugger;
		if(blk==0)
		{
			setBWFlag(1);
		}
		else
		{
			setBWFlag(0);

		}
		location.reload();
		break; //B-W
  case  57: 
		window.location="textpage_new.html?page=20";
		break; //SINGLE-LETTER
  case  48: 
		window.location="textpage_new.html?page=21";
		break; //SINGLE-COLUMN
  case  35: 
		window.location="feetSet.html";
		break; //FEET
  case  36: 
		window.location="lang_set.php.html";
		break; //LANG SETTINGS
  default : 
		
		break; //NONE
 }
}

