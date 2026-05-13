localstorage = window.localStorage;
var FEET_LOCAL_STORAGE = 'feetvalue';
var LANGUAGE1 = 'language 1';
var LANGUAGE2 = 'language 2';
var LANGUAGE3 = 'language 3';
var LEFT_COLOR = 'left color';
var RIGHT_COLOR = 'right color';
var FEET_FLAG = 'feet flag';
var UNIT_FLAG = 'unit flag';
var DECIMAL_FLAG = 'decimal flag';
var MAIN_FLAG = 'main flag';
var CONTRAST_VALUE = 'contrast value';
var BWFLAG = 'black and white';
//var SOUNDFLAG = 'sound';
var True = 1;
var False = 0;
var KEY_VALUE='serial id';

function showAlert() {
	alert('hai');
}

// function setSoundFlag(argument) {
// 	// body...
// 	localstorage.setItem(SOUNDFLAG , argument);
// }

// function getSoundFlag() {
// 	// body...
// 	debugger;
// 	if(localstorage.getItem(SOUNDFLAG)){

// 	}else{
// 		localstorage.setItem(SOUNDFLAG , 1);
// 	}
// 	return localstorage.getItem(SOUNDFLAG);
// }

function setDefault(){
	setSoundFlag(1);
	setKeyId(0);
	setContrastValue(1);
	setLanguage1("TAMIL");
	setLanguage2("HINDI");
	setLanguage3("TELUGU");
	setFeet(8);
	setMainFlag(0);
	setDecimalFlag(1);
	setUnitFlag(1);
	setFeetFlag(1);
	setLeftColor('#ff0000');
	setRightColor('#00ff00');	
	setBWFlag(0);
}

function getBWFlag(){
	if(localstorage.getItem(BWFLAG)){

	}else{
		localstorage.setItem(BWFLAG , 0);
	}
	return localstorage.getItem(BWFLAG);
}

function setBWFlag(argument) {
	// body...
	localstorage.setItem(BWFLAG , argument);
}

function getContrastValue () {
	// body...
	if(localstorage.getItem(CONTRAST_VALUE)){

	}else{
		localstorage.setItem(CONTRAST_VALUE , 1);
	}
	return localstorage.getItem(CONTRAST_VALUE);
}

function setContrastValue (argument) {
	// body...
	localstorage.setItem(CONTRAST_VALUE , argument);
}

function setKeyId (argument) {
	// body...
	localstorage.setItem(KEY_VALUE , argument);
}

function getKeyId () {
	if(localstorage.getItem(KEY_VALUE)){

	}else{
		localstorage.setItem(KEY_VALUE, 0);
	}
	return localstorage.getItem(KEY_VALUE);
}



function getMainFlag () {
	// body...
	if(localstorage.getItem(MAIN_FLAG)){

	}else{
		localstorage.setItem(MAIN_FLAG , 0);
	}
	if(localstorage.getItem(MAIN_FLAG) == 0){
		return False;
	}else{
		return True;
	}
}

function setMainFlag (argument) {
	// body...
	localstorage.setItem(MAIN_FLAG , argument);
}

function getFeet(){

	if(localstorage.getItem(FEET_LOCAL_STORAGE)){

	}else{
		setFeet('8');
	}
	return localstorage.getItem(FEET_LOCAL_STORAGE);
}

function setFeet(argument) {
	// body...
	localstorage.setItem(FEET_LOCAL_STORAGE , argument);
}

function getLanguage1(){
	var d1 = localstorage.getItem(LANGUAGE1);
	if(d1){

	}
	else{
		localstorage.setItem(LANGUAGE1 , "TAMIL");
	}
	return localstorage.getItem(LANGUAGE1);
}
function getLanguage2(){
	var d1 = localstorage.getItem(LANGUAGE2);
	if(d1){

	}
	else{
		localstorage.setItem(LANGUAGE2 , "TELUGU");
	}
	return localstorage.getItem(LANGUAGE2);
}
function getLanguage3(){
	var d1 = localstorage.getItem(LANGUAGE3);
		if(d1){

	}
	else{
		localstorage.setItem(LANGUAGE3 , "HINDI");
	}
	return localstorage.getItem(LANGUAGE3);
}
function setFeet(data){
	localstorage.setItem(FEET_LOCAL_STORAGE , data);
}

function setLanguage1(data1) {
	localstorage.setItem(LANGUAGE1 , data1);
}

function setLanguage2(data1) {

	localstorage.setItem(LANGUAGE2 , data1);
}
function setLanguage3(data1) {
	localstorage.setItem(LANGUAGE3 , data1);
}

function getLeftColor() {
	// body...
	if(localstorage.getItem(LEFT_COLOR)){

	}else{
		localstorage.setItem(LEFT_COLOR , '#ff0000');
	}
	return localstorage.getItem(LEFT_COLOR);
}
function getRightColor() {
	// body...
	if(localstorage.getItem(RIGHT_COLOR)){

	}else{
		localstorage.setItem(RIGHT_COLOR , '#00ff00');
	}
	return localstorage.getItem(RIGHT_COLOR);
}

function setLeftColor(data) {
	// body...
	localstorage.setItem(LEFT_COLOR , data);
}
function setRightColor(data) {
	// body...
	localstorage.setItem(RIGHT_COLOR , data);
}
function setDecimalFlag(data){
	localstorage.setItem(DECIMAL_FLAG , data);
}

function getDecimalFlag(){
		if(localstorage.getItem(DECIMAL_FLAG)){

	}else{
		setDecimalFlag('1');
	}

	if(localstorage.getItem(DECIMAL_FLAG) == '1'){
		return True;
	}
	else {
		return False;
	}

	return localstorage.getItem(DECIMAL_FLAG);
}
function setUnitFlag(data){
	localstorage.setItem(UNIT_FLAG , data);
}

function getUnitFlag(){
	if(localstorage.getItem(UNIT_FLAG)){

	}else{
		setUnitFlag('1');
	}

	if(localstorage.getItem(UNIT_FLAG) == '1'){
		return True;
	}
	else {
		return False;
	}

	return localstorage.getItem(UNIT_FLAG);
}
function setFeetFlag(data){
	localstorage.setItem(FEET_FLAG , data);
}

function getFeetFlag(){
	if(localstorage.getItem(FEET_FLAG)){

	}else{
		setFeetFlag('1');
	}
	if(localstorage.getItem(FEET_FLAG) == '1'){
		return True;
	}
	else {
		return False;
	}
}