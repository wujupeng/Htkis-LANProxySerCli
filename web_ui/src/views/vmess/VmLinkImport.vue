<template>
  <el-dialog :model-value="visible" title="导入vmess链接" width="600px" @close="close">
    <el-input v-model="vmessLink" type="textarea" :rows="4" placeholder="粘贴vmess://链接" />
    <div style="margin-top:12px;display:flex;gap:8px">
      <el-button @click="parseLink" :loading="parsing">解析</el-button>
    </div>
    <div v-if="preview" style="margin-top:16px">
      <el-descriptions title="节点信息" :column="2" border size="small">
        <el-descriptions-item label="地址">{{ preview.address }}</el-descriptions-item>
        <el-descriptions-item label="端口">{{ preview.port }}</el-descriptions-item>
        <el-descriptions-item label="传输协议">{{ preview.network }}</el-descriptions-item>
        <el-descriptions-item label="TLS">{{ preview.tls }}</el-descriptions-item>
        <el-descriptions-item label="备注">{{ preview.remark }}</el-descriptions-item>
      </el-descriptions>
      <el-button type="primary" style="margin-top:12px" :loading="importing" @click="confirmImport">确认导入</el-button>
    </div>
    <template #footer>
      <el-button @click="close">关闭</el-button>
    </template>
  </el-dialog>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import { ElMessage } from 'element-plus'
import request from '../../utils/request'

const props = defineProps<{ visible: boolean }>()
const emit = defineEmits(['update:visible', 'imported'])

const vmessLink = ref('')
const preview = ref<any>(null)
const parsing = ref(false)
const importing = ref(false)

function close() {
  vmessLink.value = ''
  preview.value = null
  emit('update:visible', false)
}

async function parseLink() {
  if (!vmessLink.value.trim()) { ElMessage.warning('请粘贴vmess链接'); return }
  parsing.value = true
  try {
    const { data } = await request.post('/api/v2rayn/vmess/parse', { vmess_link: vmessLink.value.trim() })
    preview.value = data.node
    ElMessage.success('解析成功')
  } catch (e: any) {
    preview.value = null
    ElMessage.error(e?.response?.data?.error || '解析失败')
  } finally { parsing.value = false }
}

async function confirmImport() {
  if (!vmessLink.value.trim()) return
  importing.value = true
  try {
    await request.post('/api/v2rayn/vmess/import', { vmess_link: vmessLink.value.trim() })
    ElMessage.success('节点已导入')
    preview.value = null
    vmessLink.value = ''
    emit('imported')
    close()
  } catch (e: any) {
    ElMessage.error(e?.response?.data?.error || '导入失败')
  } finally { importing.value = false }
}
</script>
