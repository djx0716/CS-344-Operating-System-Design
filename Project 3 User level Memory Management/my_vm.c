#include "my_vm.h"

/* global usage variables */
char* starting_address_physical_mem;
int num_pages;
int num_bits_vpn;
int num_bits_offset;
int num_bits_outer;
int num_bits_inner;
int num_outer_entries;
int num_inner_entries;
pde_t* starting_address_pde;
pte_t* starting_address_pte;
tlb* tlb_store;
double num_lookUp;
double num_misses;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* bitmap usage variables */
typedef unsigned char* bitmap_t;
bitmap_t physical_bitmap;
bitmap_t virtual_bitmap;
int number_bitmapArray_entries;

/* bitmap functions */
void printBit(char sth)
{
    int mask = 0x80;
    while (mask>0)
    {
    printf("%d", (sth & mask) > 0);
    mask >>= 1;
    }
    printf("\n");
}

void set_bitmap(bitmap_t b, int i) {
    int mask = 0x80;
    int which_entry = i / 8;
    int number_of_right_shift = i - (8 * which_entry);
    b[which_entry] |= mask >> number_of_right_shift;
}

void unset_bitmap(bitmap_t b, int i) {
    int mask = 0x80;
    int which_entry = i / 8;
    int number_of_right_shift = i - (8 * which_entry);
    b[which_entry] &= ~(mask >> number_of_right_shift);
}

int get_bitmap(bitmap_t b, int i) {
    int mask = 0x80;
    int which_entry = i / 8;
    int number_of_right_shift = i - (8 * which_entry);
    return b[which_entry] & (mask >> number_of_right_shift) ? 1 : 0;
}

bitmap_t create_bitmap(int n) {
    return malloc((n + 7) / 8);
}

static unsigned long get_top_bits(unsigned long value,  int num_bits)
{
    //Assume you would require just the higher order (outer)  bits,
    //that is first few bits from a number (e.g., virtual address)
    //So given an  unsigned int value, to extract just the higher order (outer)  鈥渘um_bits鈥�
    int num_bits_to_prune = 32 - num_bits; //32 assuming we are using 32-bit address
    return (value >> num_bits_to_prune);
}

//Now to extract some bits from the middle from a 32 bit number,
//assuming you know the number of lower_bits (for example, offset bits in a virtual address)

static unsigned long get_mid_bits (unsigned long value, int num_middle_bits, int num_lower_bits)
{

   //value corresponding to middle order bits we will returning.
   unsigned long mid_bits_value = 0;

   // First you need to remove the lower order bits (e.g. PAGE offset bits).
   value =    value >> num_lower_bits;


   // Next, you need to build a mask to prune the outer bits. How do we build a mask?

   // Step1: First, take a power of 2 for 鈥渘um_middle_bits鈥�  or simply,  a left shift of number 1.
   // You could try this in your calculator too.
   unsigned long outer_bits_mask =   (1 << num_middle_bits);

   // Step 2: Now subtract 1, which would set a total of  鈥渘um_middle_bits鈥�  to 1
   outer_bits_mask = outer_bits_mask-1;

   // Now time to get rid of the outer bits too. Because we have already set all the bits corresponding
   // to middle order bits to 1, simply perform an AND operation.
   mid_bits_value =  value &  outer_bits_mask;

  return mid_bits_value;

}

//Function to set a bit at "index"
// bitmap is a region where were store bitmap
static void set_bit_at_index(char *bitmap, int index)
{
    // We first find the location in the bitmap array where we want to set a bit
    // Because each character can store 8 bits, using the "index", we find which
    // location in the character array should we set the bit to.
    char *region = ((char *) bitmap) + (index / 8);

    // Now, we cannot just write one bit, but we can only write one character.
    // So, when we set the bit, we should not distrub other bits.
    // So, we create a mask and OR with existing values
    char bit = 1 << (index % 8);

    // just set the bit to 1. NOTE: If we want to free a bit (*bitmap_region &= ~bit;)
    *region |= bit;

    return;
}

//Function to get a bit at "index"
static int get_bit_at_index(char *bitmap, int index)
{
    //Same as example 3, get to the location in the character bitmap array
    char *region = ((char *) bitmap) + (index / 8);

    //Create a value mask that we are going to check if bit is set or not
    char bit = 1 << (index % 8);

    return (int)(*region >> (index % 8)) & 0x1;
}

// Input: a 32-bit number "n"
// Output: The binary of form of n
void show_bits(unsigned long n)
{
    unsigned i;
    printf("given %u, binary is ",n);
    for (i = 1 << 31; i > 0; i = i / 2)
        (n & i)? printf("1"): printf("0");
    printf("\n");
}

/*
Function responsible for allocating and setting your physical memory
*/
void set_physical_mem()
{

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating


    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them

    num_pages = MEMSIZE / PGSIZE;
    num_bits_offset = log(PGSIZE) / log(2);
    num_bits_outer = log(PGSIZE / 4) / log(2);
    num_bits_inner = 32 - num_bits_offset - num_bits_outer;
    num_bits_vpn = 32 - num_bits_offset;

    /* declear physical memory and  */
    starting_address_physical_mem = (char*)malloc(MEMSIZE);
    num_outer_entries = pow(2, num_bits_outer);
    num_inner_entries = pow(2, num_bits_inner);
    starting_address_pde = (pde_t*)malloc(num_outer_entries * sizeof(pde_t));

    /* declare bitmaps for both physical and virtual */
    physical_bitmap = create_bitmap(num_pages);
    virtual_bitmap = create_bitmap(num_pages);

    /* initialize every element in both bitmaps to 0x0 (0000 0000)*/
    number_bitmapArray_entries = (num_pages + 7) / 8;

    int i;
    for(i = 0; i < number_bitmapArray_entries; i++)
    {
        physical_bitmap[i] = 0x0;
        virtual_bitmap[i] = 0x0;
    }

    /* tlb initialization */
    tlb_store = (tlb*)malloc(TLB_ENTRIES * sizeof(tlb));

    int tlb_cleaner;

    for(tlb_cleaner = 0; tlb_cleaner < TLB_ENTRIES; tlb_cleaner++)
    {
        tlb_store[tlb_cleaner].stored_physical_addr = 0;
        tlb_store[tlb_cleaner].stored_virtual_num = -1;
    }

}


/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int
add_TLB(int tag)
{
// use only after check_tlb return zero.

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
    int which_tlb_entry;
    which_tlb_entry = tag % TLB_ENTRIES;

    tlb_store[which_tlb_entry].stored_virtual_num = tag;
    tlb_store[which_tlb_entry].stored_physical_addr = (unsigned long)starting_address_pte[tag];

    /*if((unsigned long)starting_address_pte[tag] == 0)
    {
        printf("in check_tlb something weired happened\n");
        return -1;
    }*/



    return 1;
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
unsigned long
check_TLB(int tag)
{
// tag is the big array (inner)'s index ranging from 0 to 1024^3.
// return physical addr if it is the wanted one. Otherwise return 0.

    /* Part 2: TLB lookup code here */
    int which_tlb_entry;
    unsigned long ret_phy_addr;

    which_tlb_entry = tag % TLB_ENTRIES;

    if(tlb_store[which_tlb_entry].stored_virtual_num == tag)
    {// if the current stored is the page we want
        ret_phy_addr = tlb_store[which_tlb_entry].stored_physical_addr;
    }
    else
    {
        num_misses++;
        ret_phy_addr = 0;
    }

    num_lookUp++;

    return ret_phy_addr;
}


/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void
print_TLB_missrate()
{
    double miss_rate = 0.0;

    /*Part 2 Code here to calculate and print the TLB miss rate*/
    miss_rate = num_misses / num_lookUp;

    printf("TLB miss rate %lf \n", miss_rate);
    /*
    printf("num_miss %lf\n", num_misses);
    printf("num_loopup %lf\n", num_lookUp);
    */
}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t *translate(pde_t *pgdir, void *va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */

    // First get the bits of page tables (both outer and inner)
    // critical section
    unsigned long vpn = get_mid_bits((unsigned long)va, num_bits_vpn, num_bits_offset);
    unsigned long outer_bits = get_top_bits(vpn, num_bits_outer);
    unsigned long inner_bits = get_mid_bits(vpn, num_bits_inner, 32);

    int indexOf_inner_page = outer_bits * num_inner_entries + inner_bits;

    // Check tlb first!
    unsigned long tlb_addr = check_TLB(indexOf_inner_page);
    if(tlb_addr != 0){
        unsigned long offset = get_mid_bits((unsigned long)va, num_bits_offset,32);
        tlb_addr = tlb_addr + offset;
        return (pte_t*)tlb_addr;
    }

    // tlb_addr = 0, add this tlb entry
    add_TLB(indexOf_inner_page);

    //check if this inner_page is valid
    int bit = get_bitmap(virtual_bitmap,indexOf_inner_page);
    if(bit == 0){
        // no mapping exist
        printf("in translate no mapping exist\n");
        unsigned long foo = 0x0;

        return (pte_t*)foo;
    }

    unsigned long phy_addr = starting_address_pte[indexOf_inner_page];

    // Now add offset bits to end of phy_addr (combine va's offset bits and phy_addr)

    // get the bits of offset

    unsigned long offset = get_mid_bits((unsigned long)va, num_bits_offset,32);
    phy_addr = phy_addr + offset; // This line may be wrong, depends on inner_page structure

    return (pte_t*)phy_addr;
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int
page_map(pde_t *pgdir, void *va, void *pa)
{

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */

    pte_t* phy_addr = translate(pgdir,va);

    if(phy_addr != starting_address_pte){
        // a mapping exist
        return 0;
    }

    // a mapping does not exist, map virtual to pa
    unsigned long vpn = get_mid_bits((unsigned long)va, num_bits_vpn, num_bits_offset);
    unsigned long outer_bits = get_top_bits(vpn, num_bits_outer);
    unsigned long inner_bits = get_mid_bits(vpn, num_bits_inner, 32);

    int indexOf_inner_page = outer_bits * num_inner_entries + inner_bits;

    //set physical bitmap
    pte_t* addr_pa = (pte_t*)pa;
    int i = 0;
    while(addr_pa != (pte_t*)starting_address_pte[i]){
        if(i>=num_pages){
            // might have bug, check i==num_pages?
            // can't find this addr
            return -1;
        }
        i++;
    }
    set_bitmap(physical_bitmap,i);

    // the addr we need to set that va to phy_mem
    unsigned long addr = (unsigned long)pa;

    // update outer_table and inner_table and set virtual bitmap
    addr = addr >> num_bits_offset;
    starting_address_pte[indexOf_inner_page] = addr; // bug?
    set_bitmap(virtual_bitmap,indexOf_inner_page);
    return 0;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_bytes, int num_page_needed, int* next_avaliable_outer_index,
                     int* next_avaliable_inner_index)
{

    //Use virtual address bitmap to find the next free page

    /** virtual **/

    pthread_mutex_lock(&lock);
    if(starting_address_pde[0] == 0x0)
    {
        starting_address_pte = (pte_t*)malloc(num_inner_entries * sizeof(pte_t));
        starting_address_pde[0] = (unsigned long)starting_address_pte;

        pthread_mutex_unlock(&lock);
        *next_avaliable_outer_index = 0;
        *next_avaliable_inner_index = 1;

        int j1;
        for(j1 = 0; j1 < num_page_needed; j1++)
        {
            set_bitmap(virtual_bitmap, (1 + j1));
        }

        return;
    }

    pthread_mutex_unlock(&lock);

    /** if this line is reached, malloc is not the first time use.
        Which means starting_address_pde[0] would not be null**/

    /*
    if(starting_address_pde == NULL)
    {
        printf("get_next_avail error\n");
    }*/

    int which_outer_index = 0;
    int which_inner_index = 1;
    int v_num_avaliable_pages = 0;

    pthread_mutex_lock(&lock);
    while(which_outer_index < num_outer_entries && starting_address_pde[which_outer_index] != 0x0)
    {
        while(which_inner_index < num_inner_entries)
        {
            int which_bitmap_index = which_outer_index * num_inner_entries + which_inner_index;
            int v_bit_at_position = get_bitmap(virtual_bitmap, which_bitmap_index);

            if(v_bit_at_position == 0)
            {
                v_num_avaliable_pages++;

                if(v_num_avaliable_pages >= num_page_needed)
                {
                    *next_avaliable_inner_index = which_inner_index - num_page_needed + 1;

                    int i3;
                    for(i3 = 0; i3 < num_page_needed; i3++)
                    {
                        set_bitmap(virtual_bitmap, (which_bitmap_index - i3));
                    }
                    *next_avaliable_outer_index = which_outer_index;
                    pthread_mutex_unlock(&lock);
                    return;
                }
            }
            else
            {
                v_num_avaliable_pages = 0;
            }

            which_inner_index++;
        }

        which_inner_index = 0;
        which_outer_index++;
    }


/** at this point, the existing outer page are all filled up.
    need to realloc the inner page table array **/

    if(which_outer_index == num_outer_entries)
    {
        printf("get_next_avail error: no enough virtual memory\n");
        pthread_mutex_unlock(&lock);
    }
    else
    {
        /* realloc inner array */
        int new_inner_size = (which_outer_index + 2) * num_inner_entries;
        starting_address_pte = (pte_t*)realloc((void*)starting_address_pte, new_inner_size);

        if(starting_address_pte == 0x0)
        {
            printf("get_next_avail realloc failed");
        }

        /* store the starting address into outer */
        int new_inner_index = which_outer_index * num_inner_entries;
        starting_address_pde[which_outer_index] = (unsigned long)(starting_address_pte + new_inner_index);

        *next_avaliable_outer_index = which_outer_index;
        *next_avaliable_inner_index = 0;
        int i2;
        for (i2 = 0; i2 < num_page_needed; i2++)
        {
            set_bitmap(virtual_bitmap, new_inner_index);
            new_inner_index++;
        }
        pthread_mutex_unlock(&lock);
        return;

    }

}


/* Function responsible for allocating pages
and used by the benchmark
*/
void *a_malloc(unsigned int num_bytes)
{

    /*
     * HINT: If the physical memory is not yet initialized, then allocate and initialize.
     */

   /*
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will
    * have to mark which physical pages are used.
    */

     /* calculate how many pages it needs */
    int has_remainder = num_bytes % PGSIZE;
    int num_page_needed;

    if(num_bytes >= PGSIZE && has_remainder == 0)
    {//need exactly a page
        num_page_needed = num_bytes / PGSIZE;
    }
    else
    {
        num_page_needed = (num_bytes / PGSIZE) + 1;
    }

    int just_a_number = -1;
    int just_a_number2 = -1;

    int* next_avaliable_outer_index;
    int* next_avaliable_inner_index;

    next_avaliable_outer_index = &just_a_number;
    next_avaliable_inner_index = &just_a_number2;

    pthread_mutex_lock(&lock);
    if (starting_address_physical_mem == NULL)
    {
        set_physical_mem();
    }
    pthread_mutex_unlock(&lock);

    get_next_avail((int)num_bytes, num_page_needed, next_avaliable_outer_index,
                    next_avaliable_inner_index);

    /* virtual done, do physical */
    int num_phy_page_found = 0;
    int count = 0;
    int bit_at_pos;
    int virtual_arr_index;
    int just_a_value = *next_avaliable_inner_index;


    while(count < num_pages)
    {
        bit_at_pos = get_bitmap(physical_bitmap, count);

        if(bit_at_pos == 0)
        {
            virtual_arr_index = (*next_avaliable_outer_index) * num_inner_entries + just_a_value;
            just_a_value = just_a_value + 1;
            pthread_mutex_lock(&lock);
            starting_address_pte[virtual_arr_index] = (unsigned long)(starting_address_physical_mem + (count * PGSIZE));
            set_bitmap(physical_bitmap, count);
            pthread_mutex_unlock(&lock);
            num_phy_page_found++;

            if(num_phy_page_found == num_page_needed)
            {
                break;
            }
        }

        count++;
    }

    unsigned long va = va_generator((unsigned long)*next_avaliable_outer_index,
                                    (unsigned long)*next_avaliable_inner_index);

    /* tlb */
    int num_page_add_tlb;
    int which_virtual_page_tlb = (*next_avaliable_outer_index) * num_inner_entries + (*next_avaliable_inner_index);

    for(num_page_add_tlb = 0; num_page_add_tlb < num_page_needed; num_page_add_tlb++)
    {
       pthread_mutex_lock(&lock);
       add_TLB(which_virtual_page_tlb);
       pthread_mutex_unlock(&lock);
       which_virtual_page_tlb++;
    }

    void* ret_val;
    ret_val = (void*)va;
    return ret_val;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void a_free(void *va, int size) {

    /* Part 1: Free the page table entries starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the
     * memory from "va" to va+size is valid.
     *
     * Part 2: Also, remove the translation from the TLB
     */

     // Part 1

     // calculate number of pages need to free
     int num_free_pages = size/PGSIZE;

     if(size%PGSIZE > 0)    num_free_pages++;
     if(size != 0 && num_free_pages == 0)   num_free_pages = 1; // for free bytes less than a page size

     unsigned long vpn = get_mid_bits((unsigned long)va, num_bits_vpn, num_bits_offset);
     unsigned long outer_bits = get_top_bits(vpn, num_bits_outer);
     unsigned long inner_bits = get_mid_bits(vpn, num_bits_inner, 32);

     // start inner page table index of va
     int indexOf_inner_page = outer_bits * num_inner_entries + inner_bits;

     // unset virtual bitmap
     pthread_mutex_lock(&lock);
     int i;
     for(i = 0;i<num_free_pages;i++){

        // free 3 pages
        int bit = get_bitmap(virtual_bitmap,indexOf_inner_page+i);
        if(bit == 0){
            // this page is not allocated yet
            // dont know what to do yet
            printf("bit = 0, fucked up?\n");
        }

        // unset vitrual bit, malloc so it can be allocated in the future
        unset_bitmap(virtual_bitmap,indexOf_inner_page+i);

        // also remove this tlb translation
        tlb_store[indexOf_inner_page+i].stored_physical_addr = 0;
        tlb_store[indexOf_inner_page+i].stored_virtual_num = -1;

     }

     // unset physcial bitmap
     // first find va's index in physical bitmap
     // clear phy_bitmap one by one

     i = 0;
     while(i<num_free_pages){
        pte_t* phy_addr = (pte_t*)starting_address_pte[indexOf_inner_page+i];
        // find the index of this phy_page
        int index_phy_mem = 0;
        unsigned long temp_phy_addr = (unsigned long)starting_address_physical_mem;
        while((unsigned long)phy_addr!=temp_phy_addr){
            // might have infinite loop here
            index_phy_mem++;
            temp_phy_addr = (unsigned long)(starting_address_physical_mem + index_phy_mem*PGSIZE);
        }

        int bit = get_bitmap(physical_bitmap,index_phy_mem);
        if(bit == 0){
            // this page is not allocated yet
            // dont know what to do yet
            printf("bit = 0, fucked up?\n");
        }
        unset_bitmap(physical_bitmap,index_phy_mem);
        i++;

     }
     pthread_mutex_unlock(&lock);

     return;


}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
*/
void put_value(void *va, void *val, int size) {

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */

     unsigned long vpn = get_mid_bits((unsigned long)va, num_bits_vpn, num_bits_offset);
     unsigned long outer_bits = get_top_bits(vpn, num_bits_outer);
     unsigned long inner_bits = get_mid_bits(vpn, num_bits_inner, 32);

     // start inner page table index of va
     int indexOf_inner_page = outer_bits * num_inner_entries + inner_bits;

     pte_t* current_page_head = (pte_t*)starting_address_pte[indexOf_inner_page];

     pte_t* current_page_tail = (pte_t*)((unsigned long)current_page_head + PGSIZE);

     pthread_mutex_lock(&lock);
     pte_t* current_page = (pte_t*)translate(starting_address_pde,va);
     pthread_mutex_unlock(&lock);

     // number of bytes can be stored in this page
     unsigned long num_bytes_left = (current_page_tail - current_page)*4; // magic number


     if(size <= num_bytes_left){
        pthread_mutex_lock(&lock);

        memcpy((void*)current_page,val,size);

        pthread_mutex_unlock(&lock);
        return;
     }


     int bytes_offset = 0;
     // size > num_bytes_left
     char* foo_val = (char*)val;

     pthread_mutex_lock(&lock);

     while(size != 0){
        if(size <= num_bytes_left){
            memcpy((void*)current_page,(void*)foo_val+bytes_offset,size);
            size = 0;
        }
        else{
            //size >= num_bytes left
            if(get_bitmap(virtual_bitmap,indexOf_inner_page) == 0){
                printf("bitmap fucked up in put_val\n");
            }
            memcpy((void*)current_page,(void*)foo_val+bytes_offset,num_bytes_left);
            size -= num_bytes_left;
            bytes_offset += num_bytes_left;
            indexOf_inner_page++;
            current_page = (pte_t*)starting_address_pte[indexOf_inner_page];

            num_bytes_left = PGSIZE;

        }
     }
     pthread_mutex_unlock(&lock);
     return;
}

/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */

    unsigned long vpn = get_mid_bits((unsigned long)va, num_bits_vpn, num_bits_offset);
    unsigned long outer_bits = get_top_bits(vpn, num_bits_outer);
    unsigned long inner_bits = get_mid_bits(vpn, num_bits_inner, 32);

    // start inner page table index of va
    int indexOf_inner_page = outer_bits * num_inner_entries + inner_bits;

    pte_t* current_page_head = (pte_t*)starting_address_pte[indexOf_inner_page];

    pte_t* current_page_tail = (pte_t*)((unsigned long)current_page_head + PGSIZE);

    pthread_mutex_lock(&lock);
    pte_t* current_page = (pte_t*)translate(starting_address_pde,va);
    pthread_mutex_unlock(&lock);

    // number of bytes can be stored in this page
    unsigned long num_bytes_left = (current_page_tail - current_page)*4;

    if(size <= num_bytes_left){
        pthread_mutex_lock(&lock);
        memcpy(val,(void*)current_page,size);
        pthread_mutex_unlock(&lock);
        return;
    }


    int bytes_offset = 0;
    // size > num_bytes_left
    char* foo_val = (char*)val;

    pthread_mutex_lock(&lock);

    while(size != 0){
        if(size <= num_bytes_left){
            memcpy((void*)foo_val+bytes_offset,(void*)current_page,size);
            size = 0;
        }
        else{
            //size >= num_bytes left
            if(get_bitmap(virtual_bitmap,indexOf_inner_page) == 0){
                printf("bitmap fucked up in put_val\n");
            }
            memcpy((void*)foo_val+bytes_offset,(void*)current_page,size);
            size -= num_bytes_left;
            bytes_offset += num_bytes_left;
            indexOf_inner_page++;
            current_page = (pte_t*)starting_address_pte[indexOf_inner_page];

            num_bytes_left = PGSIZE;

        }
    }
    pthread_mutex_unlock(&lock);
    return;
}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer)
{

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
     * matrix accessed. Similar to the code in test.c, you will use get_value() to
     * load each element and perform multiplication. Take a look at test.c! In addition to
     * getting the values from two matrices, you will perform multiplication and
     * store the result to the "answer array"
     */

    int i;
    int j;
    int k;

    unsigned long address_a = (unsigned long)mat1;
    unsigned long address_b = (unsigned long)mat2;
    unsigned long address_c = (unsigned long)answer;

    int a = 0;
    int b = 0;
    int c = 0;

    for(i = 0; i < size; i++)
    {
        for(j = 0; j < size; j++)
        {
            for(k = 0; k < size; k++)
            {
                address_a = address_a + (i * size + k) * sizeof(int);
                address_b = address_b + (k * size + j) * sizeof(int);

                get_value((void*)address_a, &a, sizeof(int));
                get_value((void*)address_b, &b, sizeof(int));

                c += a * b;

                address_a = (unsigned long)mat1;
                address_b = (unsigned long)mat2;
            }

            address_c += (size * i + j) * sizeof(int);
            put_value((void*)address_c, &c, sizeof(int));
            c = 0;
            address_c = (unsigned long)answer;
        }
    }

}

unsigned long va_generator(unsigned long outer, unsigned long inner)
{
    int num_shift_outer = 32 - num_bits_outer;
    int num_shift_inner = num_bits_offset;

    int i;
    for(i = 0; i < num_shift_outer; i++)
    {
        outer = outer << 1;
    }

    for(i = 0; i < num_shift_inner; i++)
    {
        inner = inner << 1;
    }

    return (outer | inner);
}
