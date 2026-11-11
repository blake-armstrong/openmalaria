//! Pure-Rust ports of GSL 2.8 Gaussian/lognormal cumulative distribution functions and `gsl_sf_choose`.
#![allow(clippy::excessive_precision, clippy::inconsistent_digit_grouping)]

pub use crate::cdf_beta_gamma::{beta_p, beta_pinv, gamma_p};

const SQRT_2: f64 = std::f64::consts::SQRT_2;

/// Lanczos `ln(Gamma(z))` accurate to ~1e-15 across `(0, +inf)`.
#[must_use]
pub fn ln_gamma(z: f64) -> f64 {
    const COEFFS: [f64; 9] = [
        0.999_999_999_999_809_9,
        676.520_368_121_885_1,
        -1259.139_216_722_402_8,
        771.323_428_777_653_1,
        -176.615_029_162_140_6,
        12.507_343_278_686_905,
        -0.138_571_095_265_720_12,
        9.984_369_578_019_572e-6,
        1.505_632_735_149_311_6e-7,
    ];
    if z < 0.5 {
        return std::f64::consts::PI.ln()
            - (std::f64::consts::PI * z).sin().ln()
            - ln_gamma(1.0 - z);
    }
    let z = z - 1.0;
    let mut x = COEFFS[0];
    for (i, &c) in COEFFS.iter().enumerate().skip(1) {
        x += c / (z + i as f64);
    }
    let t = z + 7.5;
    0.5 * (2.0 * std::f64::consts::PI).ln() + (z + 0.5) * t.ln() - t + x.ln()
}

/// Complementary error function `erfc(x)` accurate to `< 1e-15` (Cody 1969 rational minimax).
#[must_use]
pub fn erfc_exact(x: f64) -> f64 {
    let ax = x.abs();
    if ax <= 0.46875 {
        const A: [f64; 5] = [
            3.161_123_743_870_565_6,
            113.864_154_151_050_16,
            377.485_237_685_302,
            3209.377_589_138_469_5,
            0.185_777_706_184_603_15,
        ];
        const B: [f64; 4] = [
            23.601_290_952_344_12,
            244.024_637_934_444_17,
            1282.616_526_077_372_3,
            2844.236_833_439_170_6,
        ];
        let y = ax * ax;
        let xnum = (((A[4] * y + A[0]) * y + A[1]) * y + A[2]) * y + A[3];
        let xden = (((y + B[0]) * y + B[1]) * y + B[2]) * y + B[3];
        let erf_val = x * (xnum / xden);
        return 1.0 - erf_val;
    }
    let res = if ax <= 4.0 {
        const C: [f64; 9] = [
            0.564_188_496_988_670_1,
            8.883_149_794_388_376,
            66.119_190_637_141_62,
            298.635_138_197_400_13,
            881.952_221_241_769_1,
            1712.047_612_634_070_6,
            2051.078_377_826_071_5,
            1230.339_354_797_997_2,
            2.153_115_354_744_038_5e-8,
        ];
        const D: [f64; 8] = [
            15.744_926_110_709_835,
            117.693_950_891_312_5,
            537.181_101_862_009_9,
            1621.389_574_566_690_2,
            3290.799_235_733_459_6,
            4362.619_090_143_247,
            3439.367_674_143_721_6,
            1230.339_354_803_749_5,
        ];
        let mut xnum = C[8] * ax;
        let mut xden = ax;
        for i in 0..7 {
            xnum = (xnum + C[i]) * ax;
            xden = (xden + D[i]) * ax;
        }
        let sq = (ax * 16.0).floor() / 16.0;
        let del = (ax - sq) * (ax + sq);
        (-sq * sq).exp() * (-del).exp() * ((xnum + C[7]) / (xden + D[7]))
    } else if ax < 26.6 {
        const P: [f64; 6] = [
            0.305_326_634_961_232_34,
            0.360_344_899_949_804_44,
            0.125_781_726_111_229_25,
            0.016_083_785_148_742_277,
            6.587_491_615_298_378e-4,
            0.016_315_387_137_302_098,
        ];
        const Q: [f64; 5] = [
            2.568_520_192_289_822_4,
            1.872_952_849_923_460_4,
            0.527_905_102_951_428_4,
            0.060_518_341_312_441_32,
            0.002_335_204_976_268_691_8,
        ];
        let y = 1.0 / (ax * ax);
        let mut xnum = P[5] * y;
        let mut xden = y;
        for i in 0..4 {
            xnum = (xnum + P[i]) * y;
            xden = (xden + Q[i]) * y;
        }
        let corr = y * (xnum + P[4]) / (xden + Q[4]);
        let sq = (ax * 16.0).floor() / 16.0;
        let del = (ax - sq) * (ax + sq);
        (-sq * sq).exp() * (-del).exp() * ((1.0 / std::f64::consts::PI.sqrt() - corr) / ax)
    } else {
        0.0
    };
    if x < 0.0 { 2.0 - res } else { res }
}

/// Standard normal CDF `Phi(x)` (`gsl_cdf_ugaussian_P`).
#[must_use]
pub fn ugaussian_p(x: f64) -> f64 {
    0.5 * erfc_exact(-x / SQRT_2)
}

/// Inverse standard normal CDF `Phi^{-1}(p)` (`gsl_cdf_ugaussian_Pinv` — Wichura AS 241, 16-digit exact).
#[must_use]
pub fn ugaussian_pinv(p: f64) -> f64 {
    if p <= 0.0 {
        return f64::NEG_INFINITY;
    }
    if p >= 1.0 {
        return f64::INFINITY;
    }
    let q = p - 0.5;
    if q.abs() <= 0.425 {
        let r = 0.180625 - q * q;
        let num = (((((((2.509_080_928_730_122_7e3 * r + 3.343_057_558_358_812_8e4) * r
            + 6.726_577_092_700_870_1e4)
            * r
            + 4.592_195_393_154_987_1e4)
            * r
            + 1.373_169_376_550_946_1e4)
            * r
            + 1.971_590_950_306_551_4e3)
            * r
            + 1.331_416_678_917_843_8e2)
            * r
            + 3.387_132_872_796_366_6)
            * q;
        let den = ((((((5.226_495_278_852_854_6e3 * r + 2.872_908_573_572_194_3e4) * r
            + 3.930_789_580_009_271e4)
            * r
            + 2.121_379_430_158_659_6e4)
            * r
            + 5.394_196_021_424_751_1e3)
            * r
            + 6.871_870_074_920_579_1e2)
            * r
            + 4.231_333_070_160_091_1e1)
            * r
            + 1.0;
        return num / den;
    }

    let r_prob = if q < 0.0 { p } else { 1.0 - p };
    let r = (-r_prob.ln()).sqrt();
    let val = if r <= 5.0 {
        let r = r - 1.6;
        let num = ((((((7.745_450_142_783_414e-4 * r + 2.272_384_498_926_918_5e-2) * r
            + 2.417_807_251_774_506_1e-1)
            * r
            + 1.270_458_252_452_368_3)
            * r
            + 3.647_848_324_763_204_6)
            * r
            + 5.769_497_221_460_691_4)
            * r
            + 4.630_337_846_156_545_3)
            * r
            + 1.423_437_110_749_683_6;
        let den = ((((((1.050_750_071_644_416_8e-9 * r + 5.475_938_084_995_344_9e-4) * r
            + 1.519_866_656_361_645_7e-2)
            * r
            + 1.481_039_764_274_800_7e-1)
            * r
            + 6.897_673_349_851_000_1e-1)
            * r
            + 1.676_384_830_183_803_8)
            * r
            + 2.053_191_626_637_758_8)
            * r
            + 1.0;
        num / den
    } else {
        let r = r - 5.0;
        let num = ((((((2.010_334_399_292_288_1e-7 * r + 2.711_555_568_743_487_6e-5) * r
            + 1.242_660_947_388_078_4e-3)
            * r
            + 2.653_218_952_657_612_3e-2)
            * r
            + 2.965_605_718_285_048_9e-1)
            * r
            + 1.784_826_539_917_291_3)
            * r
            + 5.463_784_911_164_114_4)
            * r
            + 6.657_904_643_501_103_8;
        let den = ((((((2.044_263_103_389_939_8e-15 * r + 1.421_511_758_316_445_9e-7) * r
            + 1.846_318_317_510_054_7e-5)
            * r
            + 7.868_691_311_456_132_6e-4)
            * r
            + 1.487_536_129_085_061_5e-2)
            * r
            + 1.369_298_809_227_358e-1)
            * r
            + 5.998_322_065_558_879_4e-1)
            * r
            + 1.0;
        num / den
    };
    if q < 0.0 { -val } else { val }
}

/// Lognormal CDF (`gsl_cdf_lognormal_P(x, zeta, sigma)`).
#[must_use]
pub fn lognormal_p(x: f64, zeta: f64, sigma: f64) -> f64 {
    if x <= 0.0 {
        return 0.0;
    }
    ugaussian_p((x.ln() - zeta) / sigma)
}

/// Binomial coefficient `C(n, m)` in `f64` (`gsl_sf_choose(n, m)`).
#[must_use]
pub fn choose(n: u32, m: u32) -> f64 {
    if m > n {
        return 0.0;
    }
    if m == 0 || m == n {
        return 1.0;
    }
    let k = m.min(n - m);
    if n < 66 {
        let mut acc = 1.0_f64;
        for i in 1..=k {
            acc = (acc * f64::from(n - k + i)) / f64::from(i);
        }
        acc.round()
    } else {
        (ln_gamma(f64::from(n) + 1.0)
            - ln_gamma(f64::from(k) + 1.0)
            - ln_gamma(f64::from(n - k) + 1.0))
        .exp()
    }
}
