// Credit: This file is very much inspired by the work of https://github.com/ESP32-Work/ESP32-FreeRTOS-Webserver

#include <pgmspace.h>

const char PAGE_MAIN[] PROGMEM = R"=====(
 
<!DOCTYPE html>
<html lang="en" class="js-focus-visible">

<title>Desktop Height Controller</title>

  <style>
    table {
      position: relative;
      width:100%;
      border-spacing: 0px;
    }
    tr {
      border: 1px solid white;
      font-family: "Verdana", "Arial", sans-serif;
      font-size: 20px;
    }
    th {
      height: 20px;
      padding: 3px 15px;
      background-color: #343a40;
      color: #FFFFFF !important;
      }
    td {
      height: 20px;
       padding: 3px 15px;
    }
    .tabledata {
      font-size: 24px;
      position: relative;
      padding-left: 5px;
      padding-top: 5px;
      height:   25px;
      border-radius: 5px;
      color: #FFFFFF;
      line-height: 20px;
      transition: all 200ms ease-in-out;
      background-color: #00AA00;
    }
    .fanrpmslider {
      width: 30%;
      height: 55px;
      outline: none;
      height: 25px;
    }
    .bodytext {
      font-family: "Verdana", "Arial", sans-serif;
      font-size: 24px;
      text-align: left;
      font-weight: light;
      border-radius: 5px;
      display:inline;
    }
    .navbar {
      width: 100%;
      height: 50px;
      margin: 0;
      padding: 10px 0px;
      background-color: #FFF;
      color: #000000;
      border-bottom: 5px solid #293578;
    }
    .fixed-top {
      position: fixed;
      top: 0;
      right: 0;
      left: 0;
      z-index: 1030;
    }
    .navtitle {
      float: left;
      height: 50px;
      font-family: "Verdana", "Arial", sans-serif;
      font-size: 30px;
      font-weight: bold;
      line-height: 50px;
      padding-left: 20px;
    }
   .navheading {
     position: fixed;
     left: 60%;
     height: 50px;
     font-family: "Verdana", "Arial", sans-serif;
     font-size: 20px;
     font-weight: bold;
     line-height: 20px;
     padding-right: 20px;
   }
   .navdata {
      justify-content: flex-end;
      position: fixed;
      left: 70%;
      height: 50px;
      font-family: "Verdana", "Arial", sans-serif;
      font-size: 20px;
      font-weight: bold;
      line-height: 20px;
      padding-right: 20px;
   }
    .category {
      font-family: "Verdana", "Arial", sans-serif;
      font-weight: bold;
      font-size: 32px;
      line-height: 50px;
      padding: 20px 10px 0px 10px;
      color: #000000;
    }
    .heading {
      font-family: "Verdana", "Arial", sans-serif;
      font-weight: normal;
      font-size: 28px;
      text-align: left;
    }
  
    .btn {
      background-color: #444444;
      border: none;
      color: white;
      padding: 10px 20px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 16px;
      margin: 4px 2px;
      cursor: pointer;
    }
    .foot {
      font-family: "Verdana", "Arial", sans-serif;
      font-size: 20px;
      position: relative;
      height:   30px;
      text-align: center;   
      color: #AAAAAA;
      line-height: 20px;
    }
    .container {
      max-width: 1800px;
      margin: 0 auto;
    }
    table tr:first-child th:first-child {
      border-top-left-radius: 5px;
    }
    table tr:first-child th:last-child {
      border-top-right-radius: 5px;
    }
    table tr:last-child td:first-child {
      border-bottom-left-radius: 5px;
    }
    table tr:last-child td:last-child {
      border-bottom-right-radius: 5px;
    }

  </style>

  <body style="background-color: #efefef" onload="process()">
  
    <header>
      <div class="navbar fixed-top">
          <div class="container">
            <div class="navtitle">FreeRTOS Based Webserver</div>
            <div class="navdata" id = "date">mm/dd/yyyy</div>
            <div class="navheading">DATE</div><br>
            <div class="navdata" id = "time">00:00:00</div>
            <div class="navheading">TIME</div>
          </div>
      </div>
    </header>
  
    <main class="container" style="margin-top:70px">
      <div class="category">Sensor Readings</div>
      <div style="border-radius: 10px !important;">
        <div class="heading">Height Sensor Reading</div>
        <div class="tabledata" id="heightReading" style="width: 200px; margin: 20px 0; font-size: 32px; text-align: center;"></div>
      </div>
      <div style="float: right; width: 50%; padding: 0px 20px;">
        <div class="category">Cores Used</div>
        <table style="width:100%">
          <colgroup>
            <col span="1" style="background-color: #343a40; width: 20%; color: #FFFFFF;">
            <col span="1" style="background-color:rgb(0,0,0); color:#FFFFFF">
          </colgroup>
          <tr>
            <th colspan="1"><div class="heading">Core</div></th>
            <th colspan="1"><div class="heading">Status</div></th>
          </tr>
          <tr>
            <td><div class="bodytext">Core 0</div></td>
            <td><div id="core0Status" class="tabledata"></div></td>
          </tr>
          <tr>
            <td><div class="bodytext">Core 1</div></td>
            <td><div id="core1Status" class="tabledata"></div></td>
          </tr>
        </table>

        <style>
          .circle {
            width: 100px;
            height: 100px;
            border-radius: 50%;
            background-color: #343a40;
            color: #FFFFFF;
            display: flex;
            justify-content: center;
            align-items: center;
            text-align: center;
            margin-top: 10px;
          }

          .circle-data {
            font-size: 40px;
            font-weight: bold;
          }
        </style>
      </div>
    </div>
    <br>
    <div class="category">Sensor Controls</div>
    <br>
    <div class="bodytext">Up</div>
    <button type="button" class="btn" id="btn0" 
      onmousedown="ButtonPress0Start()" onmouseup="ButtonPress0Stop()" 
      ontouchstart="ButtonPress0Start()" ontouchend="ButtonPress0Stop()">
      Up
    </button>
    </div>
    <br>
    <div class="bodytext">Down</div>
    <button type="button" class="btn" id="btn1" 
      onmousedown="ButtonPress1Start()" onmouseup="ButtonPress1Stop()" 
      ontouchstart="ButtonPress1Start()" ontouchend="ButtonPress1Stop()">
      Down
    </button>
    </div>
    <br>
    <br>
    
  </main>

  <footer div class="foot" id = "temp" >Made by Magnus Aagaard</div></footer>
  
  </body>


  <script type = "text/javascript">
  
    // global variable visible to all java functions
    var xmlHttp=createXmlHttpObject();

    // function to create XML object
    function createXmlHttpObject(){
      if(window.XMLHttpRequest){
        xmlHttp=new XMLHttpRequest();
      }
      else{
        xmlHttp=new ActiveXObject("Microsoft.XMLHTTP");
      }
      return xmlHttp;
    }

    // function to handle button press from HTML code above
    // and send a processing string back to server
    // this processing string is use in the .on method
    // Press and hold for Up button
    function ButtonPress0Start() {
      var xhttp = new XMLHttpRequest();
      xhttp.open("PUT", "BUTTON_0_START", true); // async
      xhttp.send();
    }
    function ButtonPress0Stop() {
      var xhttp = new XMLHttpRequest();
      xhttp.open("PUT", "BUTTON_0_STOP", true); // async
      xhttp.send();
    }

    // Press and hold for Down button
    function ButtonPress1Start() {
      var xhttp = new XMLHttpRequest();
      xhttp.open("PUT", "BUTTON_1_START", true); // async
      xhttp.send();
    }
    function ButtonPress1Stop() {
      var xhttp = new XMLHttpRequest();
      xhttp.open("PUT", "BUTTON_1_STOP", true); // async
      xhttp.send();
    }

    // function to handle the response from the ESP
    function response(){
      var message;
      var barwidth;
      var currentsensor;
      var xmlResponse;
      var xmldoc;
      var dt = new Date();
      var color = "#e8e8e8";
     
      // get the xml stream
      xmlResponse=xmlHttp.responseXML;
  
      // get host date and time
      document.getElementById("time").innerHTML = dt.toLocaleTimeString();
      document.getElementById("date").innerHTML = dt.toLocaleDateString();
  

      // Height sensor reading
      xmldoc = xmlResponse.getElementsByTagName("HEIGHT");
      if (xmldoc.length > 0 && xmldoc[0].firstChild) {
        var heightValue = xmldoc[0].firstChild.nodeValue;
        document.getElementById("heightReading").innerHTML = heightValue + " mm";
      } else {
        document.getElementById("heightReading").innerHTML = "N/A";
      }

      // Fetch and update Core status
      xmldoc = xmlResponse.getElementsByTagName("CORE0_STATUS");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("core0Status").innerHTML = message === "1" ? "In Use" : "Not In Use";

      xmldoc = xmlResponse.getElementsByTagName("CORE1_STATUS");
      message = xmldoc[0].firstChild.nodeValue;
      document.getElementById("core1Status").innerHTML = message === "1" ? "In Use" : "Not In Use";
     }
  
    // general processing code for the web page to ask for an XML steam
    // this is actually why you need to keep sending data to the page to 
    // force this call with stuff like this
    // server.on("/xml", SendXML);
    // otherwise the page will not request XML from the ESP, and updates will not happen
    function process(){
     
     if(xmlHttp.readyState==0 || xmlHttp.readyState==4) {
        xmlHttp.open("PUT","xml",true);
        xmlHttp.onreadystatechange=response;
        xmlHttp.send(null);
      }
    // Throttled polling using setInterval
    var polling = false;
    function pollXML() {
      if (polling) return; // Only one request at a time
      polling = true;
      var req = createXmlHttpObject();
      req.onreadystatechange = function() {
        if (req.readyState == 4) {
          if (req.status == 200) {
            xmlHttp = req; // update global for response() compatibility
            response();
          }
          polling = false;
        }
      };
      req.open("PUT", "xml", true);
      req.send(null);
    }
    // Start polling every 100ms
    setInterval(pollXML, 100);
    }
  
  
  </script>

</html>



)=====";