#-*- coding:utf-8 -*-                                                                                                                    
                                                                                                                                         
import tensorflow as tf                                                                                                                  
                                                                                                                                         
a = tf.constant(3, name='const1') #�萔a                                                                                                 
b = tf.Variable(0, name='val1') #�ϐ�b                                                                                                   
                                                                                                                                         
# a��b�𑫂�                                                                                                                             
add = tf.add(a,b)                                                                                                                        
#�ϐ�b�ɑ��������ʂ��A�T�C��                                                                                                             
assign = tf.assign(b, add)                                                                                                               
c = tf.placeholder(tf.int32, name='input') #����c                                                                                        
# �A�T�C���������ʂ�c���|���Z                                                                                                            
mul = tf.multiply(assign, c)                                                                                                             
#�ϐ��̏�����                                                                                                                            
init = tf.global_variables_initializer()                                                                                                 
                                                                                                                                         
with tf.Session() as sess:                                                                                                               
    #�����������s                                                                                                                        
    sess.run(init)                                                                                                                       
    for i in range(3):                                                                                                                   
        # �|���Z���v�Z����܂ł̃��[�v�����s                                                                                             
        print(sess.run(mul, feed_dict={c:3}))         