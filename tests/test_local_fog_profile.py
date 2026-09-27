# northlight-test:
"""Offline numerical checks for the finite, tapered lamp scattering kernel."""
import math
import unittest


def integral(radius, offset, start, end, atan=math.atan):
    core = max(.75, radius * .06)
    if offset >= radius or end <= start:
        return 0.
    root = math.sqrt(radius * radius - offset * offset)
    start, end = max(start, -root), min(end, root)
    if end <= start:
        return 0.
    h = math.sqrt(offset * offset + core * core)
    return max((atan(end / h) - atan(start / h)) / h
               - (end - start) / (radius * radius + core * core), 0.)


def fast_atan(x):
    a = abs(x)
    r = a * (.785398 + .273 * (1 - a)) if a <= 1 else (
        1.570796 - (1 / a) * (.785398 + .273 * (1 - 1 / a)))
    return -r if x < 0 else r


class LocalFogProfile(unittest.TestCase):
    def test_closed_form_matches_independent_quadrature(self):
        for radius in (1., 9., 12., 20., 32.):
            core = max(.75, radius * .06)
            for fraction in (0., .2, .6, .95, .999):
                h = radius * fraction
                root = math.sqrt(radius * radius - h * h)
                for start, end in ((-root, root), (-root, 0), (-root * .2, root * .6)):
                    n = 4096
                    step = (end - start) / n
                    numeric = sum((1 / (h * h + (start + (i + .5) * step) ** 2 + core * core)
                                   - 1 / (radius * radius + core * core)) * step for i in range(n))
                    self.assertAlmostEqual(integral(radius, h, start, end), numeric, delta=1e-6)
                    # Approximate atan error is bounded in absolute energy,
                    # including grazing rays where relative error is unhelpful.
                    self.assertAlmostEqual(integral(radius, h, start, end, fast_atan), numeric, delta=.011)

    def test_soft_edge_finite_core_and_occlusion(self):
        for radius in (9., 12., 20., 32.):
            previous = float('inf')
            for i in range(1001):
                energy = integral(radius, radius * i / 1000, -radius, radius)
                self.assertTrue(math.isfinite(energy))
                self.assertGreaterEqual(energy, 0)
                self.assertLessEqual(energy, previous + 1e-12)
                previous = energy
            self.assertEqual(previous, 0)
            self.assertLess(integral(radius, radius * .999, -radius, radius), .00002)
            self.assertEqual(integral(radius, 0, -2 * radius, -radius), 0)
            self.assertEqual(integral(radius, 0, 1, 1), 0)
            # Center glow is gentler than the previous half-unit singular core.
            old_center = 4 * math.atan(math.sqrt(radius * radius - .25) * 2)
            self.assertLess(integral(radius, 0, -radius, radius), old_center)


if __name__ == '__main__':
    unittest.main()
