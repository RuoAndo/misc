5-nonlinear.py:  1: # coding: utf-8
5-nonlinear.py:  2: 
5-nonlinear.py:  3: # **[DNE-01]** ���W���[�����C���|�[�g���āA�����̃V�[�h��ݒ肵�܂��B
5-nonlinear.py:  4: 
5-nonlinear.py:  5: # In[1]:
5-nonlinear.py:  6: 
5-nonlinear.py:  7: 
5-nonlinear.py:  8: import tensorflow as tf
5-nonlinear.py:  9: import numpy as np
5-nonlinear.py: 10: import matplotlib.pyplot as plt
5-nonlinear.py: 11: from numpy.random import multivariate_normal, permutation
5-nonlinear.py: 12: import pandas as pd
5-nonlinear.py: 13: from pandas import DataFrame, Series
5-nonlinear.py: 14: 
5-nonlinear.py: 15: np.random.seed(20160615)
5-nonlinear.py: 16: tf.set_random_seed(20160615)
5-nonlinear.py: 17: 
5-nonlinear.py: 18: 
5-nonlinear.py: 19: # **[DNE-02]** �g���[�j���O�Z�b�g�̃f�[�^�𐶐����܂��B
5-nonlinear.py: 20: 
5-nonlinear.py: 21: # In[2]:
5-nonlinear.py: 22: 
5-nonlinear.py: 23: 
5-nonlinear.py: 24: def generate_datablock(n, mu, var, t):
5-nonlinear.py: 25:     data = multivariate_normal(mu, np.eye(2)*var, n)
5-nonlinear.py: 26:     df = DataFrame(data, columns=['x1','x2'])
5-nonlinear.py: 27:     df['t'] = t
5-nonlinear.py: 28:     return df
5-nonlinear.py: 29: 
5-nonlinear.py: 30: df0 = generate_datablock(30, [-7,-7], 18, 1)
5-nonlinear.py: 31: df1 = generate_datablock(30, [-7,7], 18, 0)
5-nonlinear.py: 32: df2 = generate_datablock(30, [7,-7], 18, 0)
5-nonlinear.py: 33: df3 = generate_datablock(30, [7,7], 18, 1)
5-nonlinear.py: 34: 
5-nonlinear.py: 35: df = pd.concat([df0, df1, df2, df3], ignore_index=True)
5-nonlinear.py: 36: train_set = df.reindex(permutation(df.index)).reset_index(drop=True)
5-nonlinear.py: 37: 
5-nonlinear.py: 38: 
5-nonlinear.py: 39: # **[DNE-03]** (x1, x2) �� t ��ʁX�ɏW�߂����̂�NumPy��array�I�u�W�F�N�g�Ƃ��Ď��o���Ă����܂��B
5-nonlinear.py: 40: 
5-nonlinear.py: 41: # In[3]:
5-nonlinear.py: 42: 
5-nonlinear.py: 43: 
5-nonlinear.py: 44: train_x = train_set[['x1','x2']].as_matrix()
5-nonlinear.py: 45: train_t = train_set['t'].as_matrix().reshape([len(train_set), 1])
5-nonlinear.py: 46: 
5-nonlinear.py: 47: 
5-nonlinear.py: 48: # **[DNE-04]** ��w�l�b�g���[�N�ɂ��񍀕��ފ�̃��f�����`���܂��B
5-nonlinear.py: 49: 
5-nonlinear.py: 50: # In[4]:
5-nonlinear.py: 51: 
5-nonlinear.py: 52: 
5-nonlinear.py: 53: num_units1 = 2
5-nonlinear.py: 54: num_units2 = 2
5-nonlinear.py: 55: 
5-nonlinear.py: 56: x = tf.placeholder(tf.float32, [None, 2])
5-nonlinear.py: 57: 
5-nonlinear.py: 58: w1 = tf.Variable(tf.truncated_normal([2, num_units1]))
5-nonlinear.py: 59: b1 = tf.Variable(tf.zeros([num_units1]))
5-nonlinear.py: 60: hidden1 = tf.nn.tanh(tf.matmul(x, w1) + b1)
5-nonlinear.py: 61: 
5-nonlinear.py: 62: w2 = tf.Variable(tf.truncated_normal([num_units1, num_units2]))
5-nonlinear.py: 63: b2 = tf.Variable(tf.zeros([num_units2]))
5-nonlinear.py: 64: hidden2 = tf.nn.tanh(tf.matmul(hidden1, w2) + b2)
5-nonlinear.py: 65: 
5-nonlinear.py: 66: w0 = tf.Variable(tf.zeros([num_units2, 1]))
5-nonlinear.py: 67: b0 = tf.Variable(tf.zeros([1]))
5-nonlinear.py: 68: p = tf.nn.sigmoid(tf.matmul(hidden2, w0) + b0)
5-nonlinear.py: 69: 
5-nonlinear.py: 70: 
5-nonlinear.py: 71: # **[DNE-05]** �덷�֐� loss�A�g���[�j���O�A���S���Y�� train_step�A���� accuracy ���`���܂��B
5-nonlinear.py: 72: 
5-nonlinear.py: 73: # In[5]:
5-nonlinear.py: 74: 
5-nonlinear.py: 75: 
5-nonlinear.py: 76: t = tf.placeholder(tf.float32, [None, 1])
5-nonlinear.py: 77: loss = -tf.reduce_sum(t*tf.log(p) + (1-t)*tf.log(1-p))
5-nonlinear.py: 78: train_step = tf.train.GradientDescentOptimizer(0.001).minimize(loss)
5-nonlinear.py: 79: correct_prediction = tf.equal(tf.sign(p-0.5), tf.sign(t-0.5))
5-nonlinear.py: 80: accuracy = tf.reduce_mean(tf.cast(correct_prediction, tf.float32))
5-nonlinear.py: 81: 
5-nonlinear.py: 82: 
5-nonlinear.py: 83: # **[DNE-06]** �Z�b�V������p�ӂ��āAVariable�����������܂��B
5-nonlinear.py: 84: 
5-nonlinear.py: 85: # In[6]:
5-nonlinear.py: 86: 
5-nonlinear.py: 87: 
5-nonlinear.py: 88: sess = tf.Session()
5-nonlinear.py: 89: sess.run(tf.initialize_all_variables())
5-nonlinear.py: 90: 
5-nonlinear.py: 91: 
5-nonlinear.py: 92: # **[DNE-07]** �p�����[�^�[�̍œK����2000��J��Ԃ��܂��B
5-nonlinear.py: 93: 
5-nonlinear.py: 94: # In[7]:
5-nonlinear.py: 95: 
5-nonlinear.py: 96: 
5-nonlinear.py: 97: i = 0
5-nonlinear.py: 98: for _ in range(2000):
5-nonlinear.py: 99:     i += 1
5-nonlinear.py:100:     sess.run(train_step, feed_dict={x:train_x, t:train_t})
5-nonlinear.py:101:     if i % 100 == 0:
5-nonlinear.py:102:         loss_val, acc_val = sess.run(
5-nonlinear.py:103:             [loss, accuracy], feed_dict={x:train_x, t:train_t})
5-nonlinear.py:104:         print ('Step: %d, Loss: %f, Accuracy: %f'
5-nonlinear.py:105:                % (i, loss_val, acc_val))
5-nonlinear.py:106: 
5-nonlinear.py:107: 
5-nonlinear.py:108: # **[DNE-08]** ����ꂽ�m����F�̔Z�W�Ő}�����܂��B
5-nonlinear.py:109: 
5-nonlinear.py:110: # In[8]:
5-nonlinear.py:111: 
5-nonlinear.py:112: 
5-nonlinear.py:113: train_set1 = train_set[train_set['t']==1]
5-nonlinear.py:114: train_set2 = train_set[train_set['t']==0]
5-nonlinear.py:115: 
5-nonlinear.py:116: fig = plt.figure(figsize=(6,6))
5-nonlinear.py:117: subplot = fig.add_subplot(1,1,1)
5-nonlinear.py:118: subplot.set_ylim([-15,15])
5-nonlinear.py:119: subplot.set_xlim([-15,15])
5-nonlinear.py:120: subplot.scatter(train_set1.x1, train_set1.x2, marker='x')
5-nonlinear.py:121: subplot.scatter(train_set2.x1, train_set2.x2, marker='o')
5-nonlinear.py:122: 
5-nonlinear.py:123: locations = []
5-nonlinear.py:124: for x2 in np.linspace(-15,15,100):
5-nonlinear.py:125:     for x1 in np.linspace(-15,15,100):
5-nonlinear.py:126:         locations.append((x1,x2))
5-nonlinear.py:127: p_vals = sess.run(p, feed_dict={x:locations})
5-nonlinear.py:128: p_vals = p_vals.reshape((100,100))
5-nonlinear.py:129: subplot.imshow(p_vals, origin='lower', extent=(-15,15,-15,15),
5-nonlinear.py:130:                cmap=plt.cm.gray_r, alpha=0.5)
