import numpy as np
from scipy.spatial import distance_matrix
from pygpds import ArmaVector, ArmaMatrix, phisigllik
import unittest
from arma import vector


class LoglikTest(unittest.TestCase):
    def test_vector(self):
        test = 1000
        vector = ArmaVector(range(test))
        for i in range(test):
            self.assertEqual(vector[i], i)

    def test_phisigllik(self):
        phisig = ArmaVector([1, 0.5, 2])
        yobs = ArmaMatrix(np.array(range(10)).reshape(1, -1))
        dist = ArmaMatrix(distance_matrix(np.array(range(10)).reshape(-1, 1), np.array(range(10)).reshape(-1, 1)))
        kernel = "matern"
        out = phisigllik(phisig, yobs, dist, kernel)
        self.assertAlmostEqual(out.value, -44.43196495494328)
        self.assertAlmostEqual(out.gradient, 5.43441)
        self.assertAlmostEqual(out.gradient, 7.77066)
        self.assertAlmostEqual(out.gradient, 16.7644)
