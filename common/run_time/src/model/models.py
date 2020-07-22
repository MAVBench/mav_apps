import numpy as np
import time
from scipy.optimize import curve_fit

class Model(object):

	def __init__(self, res_order, vol_order):
		self.res_order = res_order
		self.vol_order = vol_order
		self.num_coef = res_order + vol_order

		# this is the same default p0 for scipy
		self.p0 = [1] * self.num_coef

	# returns a polynomial function generated by the order settings
	# assumes len(q) == self.res_order + self.vol_order
	def _task_model(self, res, vol, q):
		#def model_gen(res, vol, q):
			# could make a function for these gens
		#	res_poly = 0
		#	exponent = self.res_order
		#	for i in range(self.res_order):
		#		res_poly += q[i] * res**(-exponent)
		#		exponent -= 1

		#	vol_poly = 0
		#	exponent = self.vol_order
		#	for i in range(self.res_order, self.vol_order):
		#		vol_poly += q[i] * vol**(exponent)
		#		exponent -= 1

		res_poly = self._poly_gen(res, self.res_order, q[:self.res_order], True)
		vol_poly = self._poly_gen(vol, self.vol_order, q[self.res_order:], False)

		return res_poly * vol_poly # coefficients need to be offset
		#return model_gen

	@staticmethod
	def _poly_gen(data, order, coeff, neg_exp=False):
		poly = 0
		exponent = order
		exp_sign = -1 if neg_exp else 1

		for i in range(order):
			poly += coeff[i] * data**(exp_sign*exponent)
			exponent -= 1
		return poly

	# this is a func for scipy's curve_fit function
	def task_model(self, x, *args):
		res = x[:,0]
		vol = x[:,1]
		q = np.array(args)
		return self._task_model(res, vol, q)
		#return (q[0]*res**(-3) + q[1]*res**(-2) + q[2]*res**(-1)) * (q[3]*vol)

	# curve_fit must be called with a p0 so it can deduce the number of coefficients
	# popt is list of model coefficients found by optimizer
	# pcov is associated covariances
	def _fit(self, xdata, ydata, p0, profile=False, **kwargs):
		if (profile):
			t0 = time.time()

		popt, pcov = curve_fit(f=self.task_model, xdata=xdata, ydata=ydata, p0=p0, **kwargs)

		elapsed = None
		if (profile):
			t1 = time.time()
			elapsed = t1 - t0

		return (popt, pcov, elapsed)

	# returns (popt, pcov, elapsed)
	#	popt is model coefficient values
	#   pcov is associated covariance
	#   elapsed is the time taken to execute if profiled
	def fit(self, res, vol, response_time, profile=False, **kwargs):
		xdata = np.vstack((res, vol)).T
		ydata = np.array(response_time)

		# p0 is needed so number of coefficients can be extrapolated
		return self._fit(xdata=xdata, ydata=ydata, p0=self.p0, profile=profile, **kwargs)

	def calc_poly(self, res, vol, coeffs):
		xdata = np.vstack(([res], [vol])).T
		result = self.task_model(xdata, *coeffs)
		return  result[0]