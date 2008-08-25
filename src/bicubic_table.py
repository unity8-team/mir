#!/usr/bin/python

def texgen():

 tex = []

 for i in range(0,512,4):

  a = i / 512.0
  a2 = a ** 2
  a3 = a ** 3

  w0 = 1 / 6.0 * (-a3 + 3 * a2 + -3 * a + 1)
  w1 = 1 / 6.0 * (3 * a3 + -6 * a2 + 4)
  w2 = 1 / 6.0 * (-3 * a3 + 3 * a2 + 3 * a + 1)
  w3 = 1 / 6.0 * a3

  tex.append(1 - (w1 / (w0 + w1)) + a)
  tex.append(1 + (w3 / (w2 + w3)) - a)
  tex.append(w0 + w1)
  tex.append(w2 + w3)

 return tex

def printrow(l, offset):

 seq = [ str(i) for i in l[offset:offset+4] ]
 return "\t" + ", ".join(seq) + ","

l = texgen()

print "static const float bicubic_tex_128[] = {"

for i in range(0, 512, 4):

 print printrow(l, i)

print "\t0 };"
