#include "os.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>



/* return 1-true if bit valid is on=1, 0 else */
int pt_level_entry_valid(uint64_t* pt_level, int i)
{
    return (((pt_level[i]&1) == 1) || (pt_level[i] == NO_MAPPING));
}

/* return 1-true if all entries are not-valid, 0 else */
int all_not_valid(uint64_t* pt_level)
{
    int i;
    for (i = 0; i < 512; i++) /* pt has 512=2^9 kids */
        if (pt_level_entry_valid(pt_level, i))
        return 0;
    return 1;
}



/* frame is 4KB, each PTE is 8B -> 512 children in page
 -> 9 bits per level. vpn is 45 bits -> we have 45/9=5 levels*/

/* according to instructions, pt and  */

/* A function to create/destroy virtual memory mappings in a page table: */
void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn)
{   
    uint64_t* pt_level; /* pointer to page table of this level*/
    uint64_t* pt_levels[5]; /* array of pointers to page tables */
    uint64_t vpn_levels[5]; /* array of virtual page numbers */
    uint64_t vpn_level; /* each level we want another 9 bits of vpn*/
    int level; /* 5 levels: 0-root, 1, 2, 3, 4-PPN(leaf) */
    int level_back;
    int nine_bits; /* 9 bits of vpn */

    pt = pt<<12; /* create addres from ppn */
    nine_bits = 0x1ff; /* 9 times one will be used to get first 9 bits of vpn_part level*/
    pt_levels[0] = (uint64_t*)phys_to_virt(pt); /*get the page table of root=pt_levels[0] */
    pt_level = pt_levels[0]; /* set the pt root */
    for (level = 0; level <= 4; level++) {
        /* calc vpn_level of level i */
        vpn_level = vpn >> (9 * (4-level)); /*make the vpn part level the first 9 bits from right*/
        vpn_level = vpn_level & nine_bits; /*get only the first 9 bits from right*/
        vpn_levels[level] = vpn_level; /* save the vpn_level */
        if (level == 4) /* if this is a level4-leaf */
        {   
            pt_level[vpn_level] = ppn; /* update leaf as ppn */
            level_back = 3;
            while (all_not_valid(pt_levels[level_back]) && (level_back!=0)) /* if after change all mapping are NOt valid */
            {                                                     /* but don't free root */
                free_page_frame(pt_levels[level_back][vpn_levels[level_back]]>>12); /* free the page table */
                pt_levels[level_back-1][vpn_levels[level_back-1]] = NO_MAPPING; /* set the mapping to NO_MAPPING */
                level_back = level_back - 1; /* go back to pt_prev and vpn_prev */
            }
            return; /* updated now done */
        }

        /* if we want to create mapping */
        if ((ppn != NO_MAPPING) && (level != 4))
        {   
            /* if there is no map yet */
            if ((pt_level[vpn_level] == NO_MAPPING) || (!pt_level_entry_valid(pt_level, vpn_level))) 
            {   
                pt_levels[level] = pt_level; /* save this level pt */
                pt_level[vpn_level] = alloc_page_frame(); /* create new pt */
                pt_level[vpn_level] = (pt_level[vpn_level]<<12) + 1; /* ppn to addres + valid=1 */
                pt_level = (uint64_t*)phys_to_virt((pt_level[vpn_level]-1)); /* get the new pt at beginning (-1) */                
            }
            else /* there is a map */
            {   /* just go deeper */    
                pt_levels[level] = pt_level; /* save this level pt */
                if (pt_level_entry_valid(pt_level, vpn_level)) /* if entry valid */
                {
                    pt_level = (uint64_t*)phys_to_virt((pt_level[vpn_level] - 1)); /* get the new pt at beginning (-1) */   
                }
                else /* if entry not valid */
                {
                    pt_level = (uint64_t*)phys_to_virt((pt_level[vpn_level])); /* get the new pt at beginning (0) */
                }
                
            }
            
        }
        else if ((ppn == NO_MAPPING) && (level != 4)) /* ppn == NO_MAPPING: if we want to destroy mapping */
        {
            /* if there is no map yet */
            if ((pt_level[vpn_level] == NO_MAPPING) || (!pt_level_entry_valid(pt_level, vpn_level))) 
            {
                return; /* nothing to do because there already no map */
            }
            else /* there is a map */
            {   
                if (pt_level_entry_valid(pt_level, vpn_level)) /* if entry valid */
                {
                    pt_level[vpn_level] = pt_level[vpn_level] - 1; /* make entry invalid */    
                }
                pt_levels[level] = pt_level; /* save this level pt */
                pt_level = (uint64_t*)phys_to_virt((pt_level[vpn_level])); /* get the new pt */
            }
        }
    }
    return;
}



/* This function returns the physical page number that vpn is mapped to,
or NO MAPPING if no mapping exists. */
uint64_t page_table_query(uint64_t pt, uint64_t vpn)
{
    uint64_t* pt_level; /* pointer to page table of this level*/
    uint64_t vpn_level; /* each level we want another 9 bits of vpn*/
    int level; /* 5 levels: 0-root, 1, 2, 3, 4-PPN(leaf) */
    int nine_bits; /* 9 bits of vpn */

    nine_bits = 0x1ff; /* 9 times one will be used to get first 9 bits of vpn_part level*/
    pt = pt<<12; /* create 12 bits in the right for bit-valid and 1-11 zero's */
    pt_level = (uint64_t*)phys_to_virt(pt); /*get the page table of root */
    for (level = 0; level <= 4; level++) {
        /* calc vpn_level of level i */
        vpn_level = vpn >> (9 * (4-level)); /*make the vpn part level the first 9 bits from right*/
        vpn_level = vpn_level & nine_bits; /*get only the first 9 bits from right*/
        if (level == 4) /* if this is a level4-leaf */
        {   
            if(pt_level[vpn_level] == NO_MAPPING) /* if bit_valid=0 or no mapping*/
            {
                return NO_MAPPING; /* no mapping */
            }   

            /* cancel 12 bits of valid-bit + 11 bits of zero */
            return pt_level[vpn_level]; /* return leaf as ppn */
        }
        /* if not leaf */
        /* if there is no map to next pt or if next pt is not valid */
        if ((pt_level[vpn_level] == NO_MAPPING) || (!pt_level_entry_valid(pt_level, vpn_level)))
        {
            return NO_MAPPING; /* return NO_MAPPING */
        }
        
        else if (pt_level_entry_valid(pt_level, vpn_level)) /* if entry valid */
        {
            pt_level = (uint64_t*)phys_to_virt(pt_level[vpn_level]-1); /* go to next level pt */
        }
        else /* if entry not valid */
        {
            pt_level = (uint64_t*)phys_to_virt(pt_level[vpn_level]);
        }

    }
    return NO_MAPPING;
    
}























