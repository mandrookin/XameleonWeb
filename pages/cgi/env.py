#!/usr/bin/env python3

import os

#print ("Content-type: text/html\r\n\r\n");
print ("<html><body>");
print ("<font size=+2>Environment</font><br/><br/>");
for param in os.environ.keys():
   print ("<b>%20s</b>: %s<br/>" % (param, os.environ[param]))

print('<br/><a href="/">Вернуться назад</a>');
print("</body></html>");
