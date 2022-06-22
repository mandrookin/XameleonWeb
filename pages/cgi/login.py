#!/usr/bin/env python3

# Import modules for CGI handling 
import os, cgi, cgitb 

# Create instance of FieldStorage 
form = cgi.FieldStorage() 

# Get data from fields
login = form.getvalue('uname')
pwd  = form.getvalue('psw')
save = form.getvalue('remember')

#print ("Content-type: text/html\r\n\r\n");
print ("<html><body>");
print ("<font size=+2>Environment</font><br/><br/>");
for param in os.environ.keys():
   print ("<b>%20s</b>: %s<br/>" % (param, os.environ[param]))

print('<br/><a href="/">Вернуться назад</a><br/>')
print('<br/> Login: ', login)
print('<br/> Password: ', pwd)
print('<br/> Remember me: ', save)
print('<br/></body></html>')
