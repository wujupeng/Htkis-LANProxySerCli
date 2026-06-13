<template>
  <div>
    <el-button type="primary" @click="showAdd = true" style="margin-bottom:16px">{{ $t('users.add') }}</el-button>
    <el-table :data="users" stripe>
      <el-table-column prop="username" :label="$t('users.username')" />
      <el-table-column :label="$t('users.expireDays')">
        <template #default="{ row }">
          {{ row.expire_time === 0 ? $t('users.neverExpire') : new Date(row.expire_time * 1000).toLocaleDateString() }}
        </template>
      </el-table-column>
      <el-table-column :label="$t('users.active')">
        <template #default="{ row }">
          <el-switch v-model="row.active" @change="(v: boolean) => toggleActive(row.username, v)" />
        </template>
      </el-table-column>
      <el-table-column :label="$t('users.actions')">
        <template #default="{ row }">
          <el-button size="small" @click="editUser(row)">{{ $t('users.edit') }}</el-button>
          <el-popconfirm :title="$t('common.deleteConfirm')" @confirm="deleteUser(row.username)">
            <template #reference><el-button size="small" type="danger">{{ $t('users.delete') }}</el-button></template>
          </el-popconfirm>
        </template>
      </el-table-column>
    </el-table>

    <el-dialog v-model="showAdd" :title="$t('users.add')">
      <el-form :model="form">
        <el-form-item :label="$t('users.username')"><el-input v-model="form.username" /></el-form-item>
        <el-form-item :label="$t('users.password')"><el-input v-model="form.password" type="password" /></el-form-item>
        <el-form-item :label="$t('users.expireDays')"><el-input-number v-model="form.days" :min="0" /></el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="showAdd = false">{{ $t('common.cancel') }}</el-button>
        <el-button type="primary" @click="addUser">{{ $t('common.save') }}</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
import { ref, reactive, onMounted } from 'vue'
import { ElMessage } from 'element-plus'
import request from '../utils/request'

const users = ref<any[]>([])
const showAdd = ref(false)
const form = reactive({ username: '', password: '', days: 0 })

async function fetchUsers() {
  const { data } = await request.get('/api/users')
  users.value = data.users || []
}

async function addUser() {
  try {
    await request.post('/api/users', form)
    ElMessage.success('OK')
    showAdd.value = false
    fetchUsers()
  } catch {}
}

async function deleteUser(username: string) {
  await request.delete(`/api/users/${username}`)
  fetchUsers()
}

async function toggleActive(username: string, active: boolean) {
  await request.put(`/api/users/${username}`, { active })
}

function editUser(row: any) {
  form.username = row.username
  form.password = ''
  form.days = 0
  showAdd.value = true
}

onMounted(fetchUsers)
</script>
