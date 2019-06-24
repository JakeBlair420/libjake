#include "patchfinder.h"
#include "a64.h"

uint64_t find_xref(jake_symbols_t syms,uint64_t start_addr,uint64_t xref_address) {
	// we search for either adr or adrp + (add/ldr)
	// this also verifies that the input register of ldr/add is the right register
	xref_address -= LIBJAKE_KERNEL_BASE; // we search on some mapping so we will just use the offset inside of that mapping as the address (when we return we slide it back)
	uint64_t search_start_offset = start_addr - LIBJAKE_KERNEL_BASE; // same here
	uint64_t adr_value = 0x0;
	char current_reg = -1;
	for (uint64_t addr = search_start_offset & 3; addr < syms->mapsize; addr += 4) {
		uint32_t ins = *(uint32_t *)(syms->map + addr);
		if (is_adr((adr_t*)&ins)) {
			adr_value = addr + get_adr_off((adr_t*)&ins);
			current_reg = ((adr_t*)&ins)->Rd;
		} else if (is_adrp((adr_t*)&ins)) {
			adr_value = (addr & ~0xfff) + get_adr_off((adr_t*)&ins);
			current_reg = ((adr_t*)&ins)->Rd;
		} else if (is_add_imm((add_imm_t*)&ins)) {
			if ((adr_value + get_add_sub_imm((add_imm_t*)&ins)) == xref_address)	{
				char myreg = ((add_imm_t*)&ins)->Rn;
				if (current_reg == myreg) {
					P_LOG_DBG("Found xref (adr(p) + add) @ %llx\n",addr+LIBJAKE_KERNEL_BASE);
					return addr + LIBJAKE_KERNEL_BASE;
				}else{
					P_LOG_DBG("Found adr(p) + add but reg isn't matching... got %d expected %d\n",myreg,current_reg);
				}
			}
		} else if (is_ldr_imm_uoff((ldr_imm_uoff_t*)&ins)) {
			if ((adr_value + get_ldr_imm_uoff((ldr_imm_uoff_t*)&ins)) == xref_address) {
				char myreg = ((ldr_imm_uoff_t*)&ins)->Rn;
				if (current_reg == myreg) {
					P_LOG_DBG("Found xref (adr(p) + ldr) @ %llx\n",addr+LIBJAKE_KERNEL_BASE);
					return addr + LIBJAKE_KERNEL_BASE;
				}else{
					P_LOG_DBG("Found adr(p) + ldr but reg isn't matching... got %d expected %d\n",myreg,current_reg);
				}
			}
		} else {
			// we only detect continues instruction pairs, because we can't make sure the same reg still contains the right value
			adr_value = 0x0;
		}
		if (adr_value == xref_address) {
			P_LOG_DBG("Found xref (adr(p)) @ %llx\n",addr+LIBJAKE_KERNEL_BASE);
			return addr + LIBJAKE_KERNEL_BASE;
		}
	}
	return 0;
}

uint64_t find_load(jake_symbols_t syms, uint64_t start_addr, uint64_t n_instr, bool backwards) {
	// we search for either adr or adrp + (add/ldr)
	// this also verifies that the input register of ldr/add is the right register
	uint64_t adr_value = 0x0;
	char current_reg = -1;
	uint64_t start_off = start_addr - LIBJAKE_KERNEL_BASE;
	bool was_adr = false;
	int step = 4;
	if (backwards) {step = -step;}
	for (uint64_t addr = start_off; addr < (start_off + n_instr*4); addr += step) {
		uint32_t ins = *(uint32_t *)(syms->map + addr);
		if (is_adr((adr_t*)&ins)) {
			adr_value = addr + get_adr_off((adr_t*)&ins);
			current_reg = ((adr_t*)&ins)->Rd;
			was_adr = true;
			step = 4; // from here we ofc have to move forwards again
		} else if (is_adrp((adr_t*)&ins)) {
			adr_value = (addr & ~0xfff) + get_adr_off((adr_t*)&ins);
			current_reg = ((adr_t*)&ins)->Rd;
			step = 4; // from here we ofc have to move forwards again
		} else if (is_add_imm((add_imm_t*)&ins)) {
			char myreg = ((add_imm_t*)&ins)->Rn;
			if (current_reg == myreg) {
				P_LOG_DBG("Found load (adr(p) + add) @ %llx\n",addr+LIBJAKE_KERNEL_BASE);
				return LIBJAKE_KERNEL_BASE + adr_value + get_add_sub_imm((add_imm_t*)&ins);
			}else{
				P_LOG_DBG("Found adr(p) + add but reg isn't matching... got %d expected %d\n",myreg,current_reg);
			}
		} else if (is_ldr_imm_uoff((ldr_imm_uoff_t*)&ins)) {
			char myreg = ((ldr_imm_uoff_t*)&ins)->Rn;
			if (current_reg == myreg) {
				P_LOG_DBG("Found load (adr(p) + ldr) @ %llx\n",addr+LIBJAKE_KERNEL_BASE);
				return LIBJAKE_KERNEL_BASE + adr_value + get_ldr_imm_uoff((ldr_imm_uoff_t*)&ins);
			}else{
				P_LOG_DBG("Found adr(p) + ldr but reg isn't matching... got %d expected %d\n",myreg,current_reg);
			}
		} else {
			// we also just except adr
			if (was_adr) {
				return LIBJAKE_KERNEL_BASE + adr_value;
			}
			// we need to start walking backwards again if we found nothing
			if (adr_value && backwards) {
				step = -4;
				addr -= 4*2; // skip two instructions upwards so that we don't hit the adr(p) again
			}
			// we only detect continues instruction pairs, because we can't make sure the same reg still contains the right value
			adr_value = 0x0;
		}
	}
	return 0;
}

uint64_t find_str_xref(jake_symbols_t syms, char * str) {
	return find_xref(syms,0,libjake_find_str(syms,str));
}

uint64_t find_swapprefix(jake_symbols_t syms) {
	return find_str_xref(syms,"/private/var/vm/swapfile");
}

uint64_t find_trustcache(jake_symbols_t syms) {
	uint64_t str_ref = find_str_xref(syms,"com.apple.MobileFileIntegrity");
	if (!str_ref) {
		P_LOG_DBG("Unable to find string for trustcache\n");
		return 0x0;
	}
	P_LOG_DBG("Found string for trustcache @ %llx\n",str_ref);
	uint64_t start = str_ref-LIBJAKE_KERNEL_BASE;
	bool found_first_bl = false;
	uint64_t target_func = 0x0;
	for (uint64_t addr = start; addr < (start + 100*4); addr+=4) {
		uint32_t ins = *(uint32_t *)(syms->map + addr);
		if (is_bl((bl_t*)&ins)) {
			if (found_first_bl) {
				target_func = get_bl_off((bl_t*)&ins) + addr;
				break;
			}
			found_first_bl = true;
		}
	}
	if (target_func == 0x0) {
		P_LOG_DBG("Unable to find the target function for trustcache\n");
		return 0x0;
	}
	P_LOG_DBG("Trustcache function @ %llx\n",target_func + LIBJAKE_KERNEL_BASE);
	return find_load(syms,target_func + LIBJAKE_KERNEL_BASE, 15, false);	
}

uint64_t find_realhost(jake_symbols_t syms) {
	// for realhost it's pretty simple because the function host_priv_self returns it and it has a symbol
	uint64_t host_priv_self = find_symbol(syms,"_host_priv_self");
	if (!host_priv_self) {
		P_LOG_DBG("Unable to find host_priv_self symbol\n");
		return 0x0;
	}
	/*
	uint32_t * ins_ptr = (uint32_t *)(syms->map + (host_priv_self - LIBJAKE_KERNEL_BASE));
	if (!is_adrp(ins_ptr)) {
		P_LOG_DBG("Expected an adrp as the first instruction\n");
		return 0x0;
	}
	uint64_t realhost = (host_priv_self & ~0xfff) + get_adr_off((adr_t*)ins_ptr);

	ins_ptr++;
	if (!is_add_imm((add_imm_t *)ins_ptr)) {
		P_LOG_DBG("Expected an add as the second instruction\n");
		return 0x0;
	}
	realhost += get_add_sub_imm((add_imm_t *)ins_ptr);
	P_LOG_DBG("Realhost is at %llx\n",realhost);
	*/
	return find_load(syms,host_priv_self,2,false); // we only give it two instructions because those have to be the first two in this function;
}

uint64_t find_zonemap(jake_symbols_t syms) {
	// for the zone map we need to search for the string zone_init: kmem_suballoc failed and then use the first adrp+add above
	uint64_t str_ref = find_str_xref(syms,"zone_init: kmem_suballoc failed");
	if (!str_ref) {
		P_LOG_DBG("Unable to find the string for zonemap\n");
		return 0x0;
	}
	P_LOG_DBG("Found string for zonemap @ %llx\n",str_ref);
	return find_load(syms,str_ref, 15, true); // move at max 15 instructions upwards and try to find the adrp+add
}
