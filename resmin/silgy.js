// --------------------------------------------------------------------------
// Show wait animation
// --------------------------------------------------------------------------
function wait()
{
    if ( !document.getElementById("wait") )
    {
        let w = document.createElement("div");
        w.id = "wait";
        w.className = "wt";     // see silgy.css
        w.style.display = "block";
        document.body.appendChild(w);
    }
    else
        document.getElementById("wait").style.display = "block";
}


// --------------------------------------------------------------------------
// Turn the spinning wheel off
// --------------------------------------------------------------------------
function wait_off()
{
    document.getElementById("wait").style.display = "none";
}


// --------------------------------------------------------------------------
// Go to link
// l = url
// --------------------------------------------------------------------------
function gt(l)
{
    wait();
    window.location.href = l;
}


// --------------------------------------------------------------------------
// Append a paragraph to the page
// --------------------------------------------------------------------------
function p(t)
{
    let p = document.createElement("p");
    if ( t ) p.innerHTML = t;
    document.body.appendChild(p);
    return p;
}


// --------------------------------------------------------------------------
// Enter or Esc key hit
// --------------------------------------------------------------------------
function ent(e)
{
    if (e.keyCode==13)   // Enter
    {
        document.getElementById("sbm").click();   // submit
        return false;
    }
    else if (e.keyCode==27)  // Esc
    {
        document.getElementById("cnc").click();   // cancel
        return false;
    }

    return true;
}


// --------------------------------------------------------------------------
// Return true if cookies are enabled
// --------------------------------------------------------------------------
function cookies()
{
    try
    {
        document.cookie = "ct=1";
        let enabled = document.cookie.indexOf("ct=") !== -1;
        document.cookie = "ct=1; expires=Thu, 01-Jan-1970 00:00:01 GMT";
        return enabled;
    }
    catch (e)
    {
        return false;
    }
}


// --------------------------------------------------------------------------
// Center div
// Call after appendChild
// d = div handle
// --------------------------------------------------------------------------
function center(d)
{
    d.style.position = "fixed";
    d.style.top = "50%";
    d.style.left = "50%";
    d.style.marginTop = -d.offsetTop/2 + "px";
    d.style.marginLeft = -d.offsetWidth/2 + "px";
}


// --------------------------------------------------------------------------
// Create modal window
// l1 = first line
// l2 = second line
// w = width (em)
// --------------------------------------------------------------------------
function mw(l1, l2, w)
{
    let d = document.createElement("div");

    if ( w )
        d.style.width = w + "em";
    else
        d.style.width = "20em";

    d.className = "mw";
    d.id = "mw";

    let s1 = document.createElement("span");

    s1.innerHTML = "<div style=\"display:flex;\">"
        + "<span style=\"width:92%;margin-top:6px;\">" + l1 + "</span>"
        + "<div style=\"width:8%;text-align:right;\"><span style=\"font-size:1.5em;cursor:pointer;\" onClick=\"mw_off();\">&#10005;</span></div>"
        + "</div><br>";

    d.appendChild(s1);

    if ( l2 )
    {
        let s2 = document.createElement("span");
        s2.innerHTML = l2;
        d.appendChild(s2);
    }

    document.body.appendChild(d);
    center(d);

    window.addEventListener("keydown", mw_off);    // allow keyboard escape
}


// --------------------------------------------------------------------------
// Remove modal window
// --------------------------------------------------------------------------
function mw_off(e)
{
    if ( e && e.keyCode!=27 ) return;
    window.removeEventListener("keydown", mw_off);
    let d = document.getElementById("mw");
    d.parentNode.removeChild(d);
}


// --------------------------------------------------------------------------
// Create Yes/No modal window
// t = text to display
// a = action if Yes
// w = width (em)
// --------------------------------------------------------------------------
function yn(t, a, w)
{
    mw(t, "<div style=\"text-align:center;\"><button class=ynb onClick=\"{"+a+"}mw_off();\">Yes</button> &nbsp; <button class=ynb onClick=\"mw_off();\">No</button></div>", w);
}


// --------------------------------------------------------------------------
// Set client's time on the server
// --------------------------------------------------------------------------
function set_tz()
{
    let dt = new Date();

    let x = new XMLHttpRequest();

    x.open("POST", "/set_tz", true);
    x.send("tz=" + Intl.DateTimeFormat().resolvedOptions().timeZone + "&tzo=" + dt.getTimezoneOffset());
}
