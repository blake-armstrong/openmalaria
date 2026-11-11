//! Bit-exact PRNG and GSL 2.8 distribution/CDF verification suite for `om-rng`.
#![forbid(unsafe_code)]

use approx::assert_relative_eq;
use om_rng::cdf::{beta_p, beta_pinv, choose, gamma_p, lognormal_p, ugaussian_p, ugaussian_pinv};
use om_rng::{LocalRng, MasterRng, OmChaCha, OmXoshiro256Plus, RngCore, RngError};
use proptest::prelude::*;

#[test]
fn chacha8_openmalaria_unique_iv_vectors() {
    let expected: [u32; 32] = [
        0x3cd0_863f, 0x1360_3b22, 0x124e_9e52, 0xd7d5_ca9f,
        0xea45_cb0e, 0xa830_d84f, 0x0d01_5250, 0x5b09_e105,
        0x2528_63ed, 0xa0c4_2af4, 0x75cc_18e3, 0x74e2_79c4,
        0x36af_c072, 0xf743_6c17, 0x1206_94e2, 0x28ba_3b58,
        0x2acf_b3bd, 0x4135_de96, 0x50ce_79c1, 0xaa90_727d,
        0x387c_c61c, 0x43c4_de0f, 0xc625_a4e7, 0x358e_10e6,
        0xc5db_18a5, 0xf4a7_0f66, 0xd5bb_6965, 0x861a_86b1,
        0x3f87_4f08, 0x64f4_85c0, 0xc878_622d, 0xa024_7a5d,
    ];
    let mut rng = OmChaCha::<8>::new(0, 0);
    for &exp in &expected {
        assert_eq!(rng.gen_u32(), exp);
    }
}

#[test]
fn chacha8_12_20_zero_key_rfc_vectors() {
    let chacha8_first4: [u8; 16] = [
        0x3e, 0x00, 0xef, 0x2f, 0x89, 0x5f, 0x40, 0xd6,
        0x7f, 0x5b, 0xb8, 0xe8, 0x1f, 0x09, 0xa5, 0xa1,
    ];
    let mut rng8 = OmChaCha::<8>::from_keysetup([0; 10]);
    for &chunk in chacha8_first4.as_chunks::<4>().0 {
        assert_eq!(rng8.gen_u32(), u32::from_le_bytes(chunk));
    }

    let chacha12_first4: [u8; 16] = [
        0x9b, 0xf4, 0x9a, 0x6a, 0x07, 0x55, 0xf9, 0x53,
        0x81, 0x1f, 0xce, 0x12, 0x5f, 0x26, 0x83, 0xd5,
    ];
    let mut rng12 = OmChaCha::<12>::from_keysetup([0; 10]);
    for &chunk in chacha12_first4.as_chunks::<4>().0 {
        assert_eq!(rng12.gen_u32(), u32::from_le_bytes(chunk));
    }

    let chacha20_first4: [u8; 16] = [
        0x76, 0xb8, 0xe0, 0xad, 0xa0, 0xf1, 0x3d, 0x90,
        0x40, 0x5d, 0x6a, 0xe5, 0x53, 0x86, 0xbd, 0x28,
    ];
    let mut rng20 = OmChaCha::<20>::from_keysetup([0; 10]);
    for &chunk in chacha20_first4.as_chunks::<4>().0 {
        assert_eq!(rng20.gen_u32(), u32::from_le_bytes(chunk));
    }
}

#[test]
fn xoshiro256plus_reference_vectors() {
    let expected: [u64; 10] = [
        5,
        211_106_232_532_999,
        211_106_635_186_183,
        9_223_759_065_350_669_058,
        9_250_833_439_874_351_877,
        13_862_484_359_527_728_515,
        2_346_507_365_006_083_650,
        1_168_864_526_675_804_870,
        34_095_955_243_042_024,
        3_466_914_240_207_415_127,
    ];
    let mut rng = OmXoshiro256Plus::from_words(1, 2, 3, 4);
    for &exp in &expected {
        assert_eq!(rng.next_u64(), exp);
    }
}

#[test]
fn gsl_2_8_exact_distribution_and_state_parity() {
    {
        let mut r = LocalRng::from_words(1, 2, 3, 4);
        assert_relative_eq!(r.gauss(0.0, 2.5), 12.057_632_511_051_938, epsilon = 1e-14);
        assert_eq!(r.core().state()[0], 9_250_551_998_855_381_762);
    }
    {
        let mut r = LocalRng::from_words(1, 2, 3, 4);
        assert_relative_eq!(r.gamma(0.5, 2.0), 3.055_893_380_681_914_7e-10, max_relative = 1e-14);
        assert_eq!(r.core().state()[0], 9_250_551_998_855_381_762);
    }
    {
        let mut r = LocalRng::from_words(1, 2, 3, 4);
        assert_relative_eq!(r.gamma(3.0, 1.5), 4.0, epsilon = 1e-14);
        assert_eq!(r.core().state()[0], 211_106_635_448_322);
    }
    {
        let mut r = LocalRng::from_words(1, 2, 3, 4);
        assert_relative_eq!(r.gamma(3.7, 1.5), 5.05, epsilon = 1e-14);
        assert_eq!(r.core().state()[0], 211_106_635_448_322);
    }
    {
        let mut r = LocalRng::from_words(1, 2, 3, 4);
        assert_relative_eq!(r.log_normal(1.2, 0.4), 3.307_434_192_436_365_7, epsilon = 1e-14);
        assert_eq!(r.core().state()[0], 9_223_547_959_118_136_069);
    }
    {
        let mut r = LocalRng::from_words(1, 2, 3, 4);
        assert_relative_eq!(r.beta(2.3, 4.1), 0.359_195_936_216_954_45, epsilon = 1e-14);
        assert_eq!(r.core().state()[0], 13_862_097_262_714_618_497);
    }
    {
        let mut r = LocalRng::from_words(1, 2, 3, 4);
        assert_eq!(r.poisson(3.2), 0);
        assert_eq!(r.core().state()[0], 7);
    }
    {
        let mut r = LocalRng::from_words(1, 2, 3, 4);
        assert_eq!(r.poisson(25.0), 21);
        assert_eq!(r.core().state()[0], 211_106_635_448_322);
    }
    {
        let mut r = LocalRng::from_words(1, 2, 3, 4);
        assert_relative_eq!(r.weibull(2.0, 1.6), 9.142_649_767_095_72, epsilon = 1e-14);
        assert_eq!(r.core().state()[0], 211_106_232_532_999);
    }
    {
        let mut r = LocalRng::from_words(1, 2, 3, 4);
        assert_eq!(r.try_poisson(f64::INFINITY), Err(RngError::InfiniteLambda));
    }
}

#[test]
fn gsl_cdf_and_choose_parity() {
    assert_relative_eq!(ugaussian_p(1.234), 0.891_398_547_878_475_7, epsilon = 1e-15);
    assert_relative_eq!(ugaussian_pinv(0.891), 1.231_863_708_734_983_3, epsilon = 1e-15);
    assert_relative_eq!(beta_pinv(0.35, 2.2, 5.1), 0.219_811_552_634_399_6, epsilon = 1e-14);
    assert_relative_eq!(gamma_p(2.5, 3.0, 1.2), 0.345_866_683_003_617, epsilon = 1e-14);
    assert_relative_eq!(lognormal_p((1.234 * 0.5 + 1.0_f64).exp(), 1.0, 0.5), ugaussian_p(1.234), epsilon = 1e-15);
    assert_eq!(choose(10, 3), 120.0);
    assert_eq!(choose(0, 0), 1.0);
    assert_eq!(choose(5, 7), 0.0);
}

proptest! {
    #[test]
    fn ugaussian_cdf_roundtrip(p in 1e-12_f64..1.0 - 1e-12) {
        let z = ugaussian_pinv(p);
        let p_back = ugaussian_p(z);
        prop_assert!((p_back - p).abs() < 1e-13);
    }

    #[test]
    fn beta_cdf_roundtrip(p in 1e-6_f64..1.0 - 1e-6, a in 0.6_f64..15.0, b in 0.6_f64..15.0) {
        let x = beta_pinv(p, a, b);
        let p_back = beta_p(x, a, b);
        prop_assert!((p_back - p).abs() < 1e-10);
    }

    #[test]
    fn chacha_discard_and_checkpoint_equivalence(seed in any::<u64>(), skip in 0u64..200, extra in 1usize..50) {
        let mut r1 = OmChaCha::<8>::new(seed, 7);
        for _ in 0..skip {
            let _ = r1.gen_u32();
        }
        let mut r2 = OmChaCha::<8>::new(seed, 7);
        r2.discard(skip);

        let mut r3 = OmChaCha::<8>::new(seed, 7);
        r3.restore_counter(r1.counter());

        for _ in 0..extra {
            let v1 = r1.gen_u32();
            prop_assert_eq!(v1, r2.gen_u32());
            prop_assert_eq!(v1, r3.gen_u32());
        }
    }

    #[test]
    fn master_seeds_independent_local_streams(seed in any::<u64>()) {
        let mut m = MasterRng::new(seed, 0);
        let mut l1 = LocalRng::from_master(&mut m);
        let l2 = LocalRng::from_master(&mut m);
        prop_assert_ne!(l1.core().state(), l2.core().state());
        let u = l1.uniform_01();
        prop_assert!((0.0..1.0).contains(&u));
    }
}
