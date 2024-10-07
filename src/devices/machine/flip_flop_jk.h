#ifndef	MAME_MACHINE_FLIP_FLOP_JK_H
#define	MAME_MACHINE_FLIP_FLOP_JK_H

#pragma once

#include <functional>

/// A classic J/K flip-flop described using positive logic only.
class flip_flop_jk
{
public:
	flip_flop_jk()
		: m_j(false)
		, m_k(true)
		, m_clk(false)
		, m_q(false)
	{}

	void set_q_cb(std::function<void(bool)> cb) {
		m_q_cb = cb;
	}

	void j_w(bool state) { m_j = state; }
	void k_w(bool state) { m_k = state; }

	void clk_w(bool state) {
		bool last_clk = m_clk;
		m_clk = state;
		if (m_clk && !last_clk) {
			tick();
		}
	}

	void pre_w(bool state) { // also known as "set"
		init();
		m_q = true;
		m_q_cb(m_q);
	}

	void clr_w(bool state) { // also known as "reset"
		init();
		m_q = false;
		m_q_cb(m_q);
	}

private:
	bool m_j;
	bool m_k;
	bool m_clk;
	bool m_q;

	std::function<void(bool)> m_q_cb;

	void tick() {
		bool last_q = m_q;

		if      ( m_j &&  m_k) m_q = !last_q;
		else if ( m_j && !m_k) m_q = true;
		else if (!m_j &&  m_k) m_q = false;
		else if (!m_j && !m_k) m_q = last_q;

		m_q_cb(m_q);
	}

	void init() {
		m_j = false;
		m_k = true;
		m_clk = false;
		m_q = false;
	}
};

#endif	/* MAME_MACHINE_FLIP_FLOP_JK_H */
