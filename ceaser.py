
##encryption key
key = 37
##encryption message
msg = bytearray("thisismysupersecretstring!@#($&*", 'ASCII')

##encrypt
for i in range(len(msg)):
    chr = msg[i]
    
    if(32 < chr < 127):
        chr = (chr -33 + key)%(127-33) + 33
        
    
    msg[i] = chr
    
print("entrypted message: ", msg.decode('ascii'))

#decrypt string
for i in range(len(msg)):
    chr = msg[i]
    
    if(32 < chr < 127):
        chr = (chr -33 - key)%(127-33) + 33
        
    msg[i] = chr
    
print("decrypted message: ", msg.decode('ascii'))
