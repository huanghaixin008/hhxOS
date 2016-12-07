# include <const.h>
# include <protect.h>
# include <proc.h>
# include <proto.h>
# include <global.h>

# include <hd.h>

static struct inode * create_file(char * path, int flags);
static int alloc_imap_bit(int dev);
static int alloc_smap_bit(int dev, int nr_sects_to_alloc);
static struct inode * new_inode(int dev, int inode_nr, int start_sect);
static void new_dir_entry(struct inode * dir_inode, int inode_nr, char * filename);

int open(const char * pathname, int flags)
{
	MESSAGE msg;

	msg.type	= OPEN;

	msg.PATHNAME	= (void*)pathname;
	msg.FLAGS	= flags;
	msg.NAME_LEN	= strlen(pathname);

	send_recv(BOTH, TASK_FS, &msg);
	assert(msg.type == SYSCALL_RET);

	return msg.FD;
}

int close(int fd)
{
	MESSAGE msg;
	msg.type   = CLOSE;
	msg.FD     = fd;

	send_recv(BOTH, TASK_FS, &msg);

	return msg.RETVAL;
}

int do_open()
{
	int fd = -1;

	char pathname[MAX_PATH];

	int flags = fs_msg.FLAGS;
	int name_len = fs_msg.NAME_LEN;
	int src = fs_msg.source;
	assert(name_len < MAX_PATH);
	phys_copy((void*)va2la(TASK_FS, pathname),
		  (void*)va2la(src, fs_msg.PATHNAME),
		  name_len);
	pathname[name_len] = 0;

	int i;
	for (i = 0; i < NR_FILES; i++) {
		if (pcaller->flip[i] == 0) {
			fd = i;
			break;
		}
	}

	if ((fd < 0) || (fd >= NR_FILES))
		panic("{FS} flip[] is full (PID: %d)", proc2pid(pcaller));

	for (i = 0;i < NR_FILE_DESC; i++)
		if (f_desc_table[i].fd_inode == 0)
			break;
	if (i >= NR_FILE_DESC)
		panic("{FS} f_desc_table[] is full (PID: %d)", proc2pid(pcaller));

	int inode_nr = search_file(pathname);

	struct inode * pin = 0;
/*
	if (inode_nr) {
		if (!(flags & O_RDWR)) {  // no O_RDWR
			printl("{FS} File exists: %s but no operation specified\n", pathname);
			return -1;
		}
		if (flags & O_CREAT) {  // has O_CREAT	
			if (!(flags & O_TRUNC)) {  // has O_CREAT but no O_TRUNC
				printl("{FS} File exists: %s\n", pathname);
				return -1;
			}
		}

		assert(((flags & O_RDWR) && !(flags & O_CREAT)) ||
			((flags & O_RDWR) && (flags & O_TRUNC)));	
		char filename[MAX_PATH];
		struct inode * dir_inode;
		if (strip_path(filename, pathname, &dir_inode) != 0)
			return -1;
		pin = get_inode(dir_inode->i_dev, inode_nr);
		
		if (flags & O_TRUNC) {
			assert(pin);
			pin->i_size = 0;
			sync_inode(pin);
		}
	} else if (flags & O_CREAT) {
		pin = create_file(pathname, flags);
	} else {
		printl("{FS} File doesn't exist: %s\n", pathname);
		return -1;
	}*/

	if (inode_nr == INVALID_INODE) { /* file not exists */
		if (flags & O_CREAT) {
			pin = create_file(pathname, flags);
		}
		else {
			printl("{FS} file not exists: %s\n", pathname);
			return -1;
		}
	}
	else if (flags & O_RDWR) { /* file exists */
		if ((flags & O_CREAT) && (!(flags & O_TRUNC))) {
			assert(flags == (O_RDWR | O_CREAT));
			printl("{FS} file exists: %s\n", pathname);
			return -1;
		}
		assert((flags ==  O_RDWR                     ) ||
		       (flags == (O_RDWR | O_TRUNC          )) ||
		       (flags == (O_RDWR | O_TRUNC | O_CREAT)));

		char filename[MAX_PATH];
		struct inode * dir_inode;
		if (strip_path(filename, pathname, &dir_inode) != 0)
			return -1;
		pin = get_inode(dir_inode->i_dev, inode_nr);
	}
	else { /* file exists, no O_RDWR flag */
		printl("{FS} file exists: %s\n", pathname);
		return -1;
	}

	if (flags & O_TRUNC) {
		assert(pin);
		pin->i_size = 0;
		sync_inode(pin);
	}

	if (pin) {
		pcaller->flip[fd] = &f_desc_table[i];
		f_desc_table[i].fd_inode = pin;

		f_desc_table[i].fd_mode = flags;
		f_desc_table[i].fd_pos = 0;

		int imode = pin->i_mode & I_TYPE_MASK;
		if (imode == I_CHAR_SPECIAL) {
			MESSAGE driver_msg;

			driver_msg.type = DEV_OPEN;
			int dev = pin->i_start_sect;
			driver_msg.DEVICE = MINOR(dev);
			assert(MAJOR(dev) == 4);
			assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);

			send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, &driver_msg);
		}
		else if (imode == I_DIRECTORY) {
			assert(pin->i_num == ROOT_INODE);
		}
		else { 
			assert(pin->i_mode == I_REGULAR);
		}
	} else {
		return -1;
	}

	return fd;
}

int do_close()
{
	int fd = fs_msg.FD;
	put_inode(pcaller->flip[fd]->fd_inode);
	pcaller->flip[fd]->fd_inode = 0;
	pcaller->flip[fd] = 0;

	return 0;
}

static struct inode * create_file(char * path, int flags)
{
	char filename[MAX_PATH];
	struct inode * dir_inode;

	if (strip_path(filename, path, &dir_inode) != 0)
		return 0;

	int inode_nr = alloc_imap_bit(dir_inode->i_dev);
	int free_sect_nr = alloc_smap_bit(dir_inode->i_dev, NR_DEFAULT_FILE_SECTS);
	struct inode * newino = new_inode(dir_inode->i_dev, inode_nr, free_sect_nr);

	new_dir_entry(dir_inode, newino->i_num, filename);

	return newino;
}

static int alloc_imap_bit(int dev)
{
	int inode_nr = 0;
	int i, j, k;

	int imap_blk0_nr = 1 + 1; /* 1 boot sector & 1 super block */
	struct super_block * sb = get_super_block(dev);

	for (i = 0; i < sb->nr_imap_sects; i++) {
		RD_SECT(dev, imap_blk0_nr + i);

		for (j = 0; j < SECTOR_SIZE; j++) {
			/* skip `11111111' bytes */
			if (fsbuf[j] == 0xFF)
				continue;

			/* skip `1' bits */
			for (k = 0; ((fsbuf[j] >> k) & 1) != 0; k++) {}

			/* i: sector index; j: byte index; k: bit index */
			inode_nr = (i * SECTOR_SIZE + j) * 8 + k;
			fsbuf[j] |= (1 << k);

			/* write the bit to imap */
			WR_SECT(dev, imap_blk0_nr + i);
			break;
		}

		return inode_nr;
	}

	/* no free bit in imap */
	panic("inode-map is probably full.\n");

	return 0;
}

static int alloc_smap_bit(int dev, int nr_sects_to_alloc)
{
	/* int nr_sects_to_alloc = NR_DEFAULT_FILE_SECTS; */

	int i; /* sector index */
	int j; /* byte index */
	int k; /* bit index */

	struct super_block * sb = get_super_block(dev);

	int smap_blk0_nr = 1 + 1 + sb->nr_imap_sects;
	int free_sect_nr = 0;

	for (i = 0; i < sb->nr_smap_sects; i++) { /* smap_blk0_nr + i :
						     current sect nr. */
		RD_SECT(dev, smap_blk0_nr + i);

		/* byte offset in current sect */
		for (j = 0; j < SECTOR_SIZE && nr_sects_to_alloc > 0; j++) {
			k = 0;
			if (!free_sect_nr) {
				/* loop until a free bit is found */
				if (fsbuf[j] == 0xFF) continue;
				for (; ((fsbuf[j] >> k) & 1) != 0; k++) {}
				free_sect_nr = (i * SECTOR_SIZE + j) * 8 +
					k - 1 + sb->n_1st_sect;
			}

			for (; k < 8; k++) { /* repeat till enough bits are set */
				assert(((fsbuf[j] >> k) & 1) == 0);
				fsbuf[j] |= (1 << k);
				if (--nr_sects_to_alloc == 0)
					break;
			}
		}

		if (free_sect_nr) /* free bit found, write the bits to smap */
			WR_SECT(dev, smap_blk0_nr + i);

		if (nr_sects_to_alloc == 0)
			break;
	}

	assert(nr_sects_to_alloc == 0);

	return free_sect_nr;
}

static struct inode * new_inode(int dev, int inode_nr, int start_sect)
{
	struct inode * new_inode = get_inode(dev, inode_nr);

	new_inode->i_mode = I_REGULAR;
	new_inode->i_size = 0;
	new_inode->i_start_sect = start_sect;
	new_inode->i_nr_sects = NR_DEFAULT_FILE_SECTS;

	new_inode->i_dev = dev;
	new_inode->i_cnt = 1;
	new_inode->i_num = inode_nr;

	sync_inode(new_inode);

	return new_inode;
}

static void new_dir_entry(struct inode * dir_inode, int inode_nr, char * filename)
{
	int dir_blk0_nr = dir_inode->i_start_sect;
	int nr_dir_blks = (dir_inode->i_size + SECTOR_SIZE) / SECTOR_SIZE;
	int nr_dir_entries = 
		dir_inode->i_size / DIR_ENTRY_SIZE;

	int m = 0;
	struct dir_entry * pde;
	struct dir_entry * new_de = 0;

	int i, j;
	for (i = 0; i < nr_dir_blks; i++) {
		RD_SECT(dir_inode->i_dev, dir_blk0_nr + i);
		
		pde = (struct dir_entry *) fsbuf;
		for (j = 0; j < SECTOR_SIZE / DIR_ENTRY_SIZE; j++, pde++) {
			if (++m > nr_dir_entries)
				break;
			if (pde->inode_nr == 0) {
				new_de = pde;
				break;
			}

		}
		if (m > nr_dir_entries || new_de)
			break;
	}

	if (!new_de) {
		new_de = pde;
		dir_inode->i_size += DIR_ENTRY_SIZE;
	}
	new_de->inode_nr = inode_nr;
	strcpy(new_de->name, filename);

	WR_SECT(dir_inode->i_dev, dir_blk0_nr + i);

	sync_inode(dir_inode);
}
